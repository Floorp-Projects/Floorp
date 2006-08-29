#!/usr/bin/python

import cse.Database
import cse.MySQLDatabase
import cse.TabularData

import mx.DateTime
import os.path

import sys
import traceback

version = "0.2"

#-----------------------------------------------------------------------------------------------------------
# f i r s t W o r d s
#-----------------------------------------------------------------------------------------------------------
def firstWords (originalString, maxCharacters = 255, maxSearchBack = 20):
  """This function returns the first maxCharacters of originalString making sure that the end of the 
     string was cut on a word break and elipsis (...) was appended.
     
     input:
       originalString - the string
       maxCharacters - the maximum number of characters to return
       maxSearchBack - if no word break was found after searching back this many characters, give up and
                       return the maximum number of characters
  """
  try:
    if len(originalString) <= maxCharacters: return originalString
    breakIndex = maxCharacters - 3
    maximumSearchBackIndex = breakIndex - maxSearchBack
    if maximumSearchBackIndex < 0:
      maximumSearchBackIndex = 0
    while breakIndex > maximumSearchBackIndex and not (originalString[breakIndex] == " " and originalString[breakIndex-1] != " "):
      breakIndex -= 1
    if breakIndex == maximumSearchBackIndex:
      breakIndex = maxCharacters - 3
    if originalString[breakIndex - 1] in ".,:;\"'!":
      breakIndex -= 1
    return "%s..." % originalString[:breakIndex]
  except TypeError, x:
    print >>sys.stderr, x
    print >>sys.stderr, "  %s passed in as a string" % originalString
    
yesNoEnumToTinyIntMapping = { "YES": 1, "NO": 0, "?":2, "DISABLED":3 }
  
def changeNoneIntoEmptyString (x):
  if x is None: return ''
  return x
  
#-----------------------------------------------------------------------------------------------------------
# c l e a n M e t a D a t a T a b l e s
#-----------------------------------------------------------------------------------------------------------
def cleanMetaDataTables (newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tclearing metadata tables..." % mx.DateTime.now()
  newDB.executeSql("delete from tags")
  newDB.executeSql("delete from appversions")
  newDB.executeSql("delete from applications")
  newDB.executeSql("delete from platforms")
  newDB.executeSql("delete from addons_users")
  newDB.executeSql("delete from users")
  if newDB.singleValueSql("select count(*) from addontypes") == 0:
    newDB.executeSql("insert into addontypes(name, created) values ('Extensions', NOW()), ('Themes', NOW())")
  newDB.commit()
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."
  
#-----------------------------------------------------------------------------------------------------------
# a p p l i c a t i o n s T o A p p l i c a t i o n s
#-----------------------------------------------------------------------------------------------------------
applicationsInsertSql = """
  insert into applications (id, guid, name, shortname, created)
  values (%s, %s, %s, %s, %s)"""
appversionsInsertSql = """
  insert into appversions (id, application_id, version, created)
  values (%s, %s, %s, %s)"""
def applicationsToApplications (oldDB, newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tbeginning applicationsToApplications..." % mx.DateTime.now()
  applicationsAlreadyInserted = {}
  for a in oldDB.executeSqlNoCache("select * from applications"):
    if a.AppName not in applicationsAlreadyInserted:
      newDB.executeManySql(applicationsInsertSql, [ (a.AppID, a.GUID, a.AppName, a.AppName, mx.DateTime.now()) ] )
      applicationsAlreadyInserted[a.AppName] = a.AppID
    newDB.executeManySql(appversionsInsertSql, [ (a.AppID, applicationsAlreadyInserted[a.AppName], a.Version, mx.DateTime.now()) ] )
  newDB.commit()
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."

#-----------------------------------------------------------------------------------------------------------
# c a t e g o r i e s T o T a g s
#-----------------------------------------------------------------------------------------------------------
tagsInsertSql = """
  insert into tags (id, name, description, addontype_id, application_id, created)
  values (%s, %s, %s, %s, %s, %s)"""
typeEnumToTypeNameMapping = {'E': 'Extensions',
                             'T': 'Themes',
                             'P': 'Plugins' }
def categoriesToTags (oldDB, newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tbeginning categoriesToTags..." % mx.DateTime.now()
  for c in oldDB.executeSqlNoCache("select * from categories"):
    newDB.executeManySql(tagsInsertSql, [ (c.CategoryID, c.CatName, c.CatDesc, 
                                           newDB.singleValueSql("select id from addontypes where name = '%s'" 
                                                                 % typeEnumToTypeNameMapping[c.CatType]),
                                           newDB.singleValueSql("select id from applications where name = '%s'" % c.CatApp),
                                           mx.DateTime.now()) ] )
  newDB.commit()
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."

  
#-----------------------------------------------------------------------------------------------------------
# o s T o P l a t f o r m s
#-----------------------------------------------------------------------------------------------------------
plaformsInsertSql = """
  insert into platforms (id, name, shortname, created)
  values (%s, %s, %s, %s)"""
def osToPlatforms (oldDB, newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tbeginning osToPlatforms..." % mx.DateTime.now()
  for o in oldDB.executeSqlNoCache("select * from os"):
    newDB.executeManySql(plaformsInsertSql, [ (o.OSID, o.OSName, o.OSName, mx.DateTime.now()) ] )
  newDB.commit()
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."
  
#-----------------------------------------------------------------------------------------------------------
# u s e r p r o f i l e s T o U s e r s
#-----------------------------------------------------------------------------------------------------------
usersInsertSql = """
  insert into users (id, firstname, lastname, email, homepage, password, emailhidden, confirmationcode, created, notes)
  values (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"""
def userprofilesToUsers (oldDB, newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tbeginning userprofilesToUsers..." % mx.DateTime.now()
  for u in oldDB.executeSqlNoCache("select * from userprofiles"):
    newDB.executeManySql(usersInsertSql, [ (u.UserID, u.UserName, u.UserName, u.UserEmail, u.UserWebsite, u.UserPass,
                                       u.UserEmailHide, u.ConfirmationCode, mx.DateTime.now(), None) ] )
  newDB.commit()
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."
  
#-----------------------------------------------------------------------------------------------------------
# c l e a n A d d o n s T a b l e s
#-----------------------------------------------------------------------------------------------------------
def cleanAddonsTables (newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tclearing addons tables..." % mx.DateTime.now()
  if "all" in workingEnvironment["addons"]:
    newDB.executeSql("delete from previews")
    newDB.executeSql("delete from addons_tags")
    newDB.executeSql("delete from applications_versions")
    newDB.executeSql("delete from files")    
    newDB.executeSql("delete from versions")
    newDB.executeSql("delete from addons_users")
    newDB.executeSql("delete from addons")
  else:
    newDB.executeSql("delete from previews where addon_id in (select id from addonSelections)")
    newDB.executeSql("delete from addons_tags where addon_id in (select id from addonSelections)")
    newDB.executeSql("delete from applications_versions where version_id in (select v.id from versions v where addon_id in (select id from addonSelections))")
    newDB.executeSql("delete from files where version_id in (select v.id from versions v where addon_id in (select id from addonSelections))")   
    newDB.executeSql("delete from versions where addon_id in (select id from addonSelections)")
    newDB.executeSql("delete from addons_users where addon_id in (select id from addonSelections)")
    newDB.executeSql("delete from addons where id in (select id from addonSelections)")
  newDB.commit()
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."

#-----------------------------------------------------------------------------------------------------------
# m a i n T o A d d O n s
#-----------------------------------------------------------------------------------------------------------
addonsInsertSql = """
  insert into addons (id, guid, name, addontype_id, created, homepage, description, averagerating,
                      weeklydownloads, totaldownloads, developercomments, summary, approvalnotes,
                      eula, privacypolicy)
  values (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)""" 
def mainToAddOns (oldDB, newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tbeginning mainToAddOns..." % mx.DateTime.now()
  if "all" in workingEnvironment["addons"]:
    sql = "select m.* from main m"
  else:
    sql = "select m.* from main m where m.ID in (select id from addonSelections)"  
  for a in oldDB.executeSqlNoCache(sql):
    #print a.ID, sql
    x = (a.ID, a.GUID, a.Name, 
         newDB.singleValueSql("select id from addontypes where name = '%s'" 
                              % typeEnumToTypeNameMapping[a.Type]), 
         mx.DateTime.now(), a.Homepage, a.Description, a.Rating, a.downloadcount,
         a.TotalDownloads, changeNoneIntoEmptyString(a.devcomments), firstWords(a.Description, 255), None, None, None)
    newDB.executeManySql(addonsInsertSql, [ x ] )
  newDB.commit()
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."
  
#-----------------------------------------------------------------------------------------------------------
# a u t h o r x r e f T o A d d o n s _ u s e r s
#-----------------------------------------------------------------------------------------------------------
addons_usersInsertSql = """
  insert into addons_users (addon_id, user_id, created)
  values (%s, %s, %s)"""
def authorxrefToAddons_users (oldDB, newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tbeginning authorxrefToAddons_users..." % mx.DateTime.now()
  if "all" in workingEnvironment["addons"]:
    sql = "select * from authorxref"
  else:
    sql = "select * from authorxref where id in (select id from addonSelections)"  
  for u in oldDB.executeSqlNoCache(sql):
    newDB.executeManySql(addons_usersInsertSql, [ (u.ID, u.UserID, mx.DateTime.now()) ] )
  newDB.commit()
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."
  

#-----------------------------------------------------------------------------------------------------------
# v e r s i o n T o V e r i o n s
#-----------------------------------------------------------------------------------------------------------
versionsInsertSql = """
  insert into versions (id, addon_id, version, dateadded, dateapproved, dateupdated, releasenotes, approved,
                        created)
  values (%s, %s, %s, %s, %s, %s, %s, %s, %s)""" 
applications_versionsInsertSql = """
  insert into applications_versions (application_id, version_id, min, max, created)
  values (%s, %s, %s, %s, %s)""" 
filesInsertSql = """
  insert into files (version_id, platform_id, filename, size, hash, created)
  values (%s, %s, %s, %s, %s, %s)"""
def versionToVerions (oldDB, newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tbeginning versionToVerions..." % mx.DateTime.now()
  if "all" in workingEnvironment["addons"]:
    sql = "select * from version"
  else:
    sql = "select * from version where id in (select id from addonSelections)"
  for v in oldDB.executeSqlNoCache(sql):
    try:
      newDB.executeManySql(versionsInsertSql, [ (v.vID, v.ID, v.Version, v.DateAdded, v.DateApproved, v.DateUpdated,
                                                 v.Notes, yesNoEnumToTinyIntMapping[v.approved], mx.DateTime.now()) ] )
      newDB.executeManySql(applications_versionsInsertSql, [ (v.AppID, v.vID, v.MinAppVer, v.MaxAppVer, 
                                                              mx.DateTime.now()) ] )
      newDB.executeManySql(filesInsertSql, [ (v.vID, v.OSID, os.path.basename(v.URI), v.Size, v.hash, mx.DateTime.now()) ] )
      newDB.commit()
    except KeyboardInterrupt, x:
      raise x
    except Exception, x:
      print >>sys.stderr, "%s\tWARNING -- version ID %d of addon %s for application %d fails to migrate.\n\t\t\t%s" % (mx.DateTime.now(), v.vID, v.ID, v.AppID, x)
      newDB.rollback()

  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."

#-----------------------------------------------------------------------------------------------------------
# c a t e g o r y x r e f T o A d d o n s _ t a g s
#-----------------------------------------------------------------------------------------------------------
addons_tagsInsertSql = """
  insert into addons_tags (addon_id, tag_id)
  values (%s, %s)""" 
def categoryxrefToAddons_tags (oldDB, newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tbeginning categoryxrefToAddons_tags..." % mx.DateTime.now()
  if "all" in workingEnvironment["addons"]:
    sql = "select * from categoryxref"
  else:
    sql = "select * from categoryxref where id in (select id from addonSelections)"  
  for c in oldDB.executeSqlNoCache(sql):
    newDB.executeManySql(addons_tagsInsertSql, [ (c.ID, c.CategoryID) ] )
  newDB.commit()
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."

#-----------------------------------------------------------------------------------------------------------
# p r e v i e w s T o P r e v i e w s
#-----------------------------------------------------------------------------------------------------------
previewsInsertSql = """
  insert into previews (id, filename, addon_id, caption, highlight, created)
  values (%s, %s, %s, %s, %s, %s)""" 
def previewsToPreviews (oldDB, newDB, workingEnvironment):
  if workingEnvironment.has_key("verbose"): print "%s\tbeginning previewsToPreviews..." % mx.DateTime.now()
  if "all" in workingEnvironment["addons"]:
    sql = "select * from previews"
  else:
    sql = "select * from previews where id in (select id from addonSelections)"  
  for p in oldDB.executeSqlNoCache(sql):
    newDB.executeManySql(previewsInsertSql, [ (p.PreviewID, p.PreviewURI, p.ID, p.caption, yesNoEnumToTinyIntMapping[p.preview], mx.DateTime.now()) ] )  
  newDB.commit()
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."

#-----------------------------------------------------------------------------------------------------------
# s e t u p A d d o n S e l e c t i o n T a b l e
#-----------------------------------------------------------------------------------------------------------
def setupAddonSelectionTable(oldDB, newDB, workingEnvironment):
  if "all" in workingEnvironment["addons"]: return
  
  if workingEnvironment.has_key("verbose"): print "%s\tbeginning setupAddonSelectionTable..." % mx.DateTime.now()
  
  #cleanupAddonSelectionTable(oldDB, newDB, workingEnvironment)
  oldDB.executeSql("create table addonSelections (id int, primary key (id))")
  newDB.executeSql("create table addonSelections (id int, primary key (id))")

  try:
    listOfAddons = [x.strip() for x in workingEnvironment["addons"].split(",")]
    if workingEnvironment.has_key("not"):
      for anAddon in oldDB.executeSqlNoCache("select ID from main"):
        if str(anAddon.ID) not in listOfAddons:
          oldDB.executeSql("insert into addonSelections (id) values (%d)" % anAddon.ID)
          newDB.executeSql("insert into addonSelections (id) values (%d)" % anAddon.ID)
    else:
      oldDB.executeManySql("insert into addonSelections (id) values (%s)", listOfAddons)
      newDB.executeManySql("insert into addonSelections (id) values (%s)", listOfAddons)
  except Exception, x:
    cleanupAddonSelectionTable(oldDB, newDB, workingEnvironment)
    raise x
    
  if workingEnvironment.has_key("verbose"): print "\t\t\tdone."
  
#-----------------------------------------------------------------------------------------------------------
# c l e a n u p A d d o n S e l e c t i o n T a b l e
#-----------------------------------------------------------------------------------------------------------
def cleanupAddonSelectionTable (oldDB, newDB, workingEnvironment):
  if "all" in workingEnvironment["addons"]: return
  oldDB.executeSql("drop table if exists addonSelections")
  newDB.executeSql("drop table if exists addonSelections")


#===========================================================================================================
# m a i n
#===========================================================================================================
if __name__ == "__main__":

  import cse.ConfigurationManager
  
  try:
  
    options = [ ('?',  'help', False, None, 'print this message'), 
                ('c',  'config', True, './migration.conf', 'specify the location and name of the config file'),
                (None, 'oldDatabaseName', True, "", 'the name of the old database within the server'),
                (None, 'oldServerName', True, "", 'the name of the old database server'),
                (None, 'oldUserName', True, "", 'the name of the user in the old database'),
                (None, 'oldPassword', True, "", 'the password for the user in the old database'),
                (None, 'newDatabaseName', True, "", 'the name of the new database within the server'),
                (None, 'newServerName', True, "", 'the name of the new database server'),
                (None, 'newUserName', True, "", 'the name of the user in the new database'),
                (None, 'newPassword', True, "", 'the password for the user in the new database'),
                ('a',  'addons', True, "all", 'a quoted comma delimited list of the ids of addons OR the word "all"'),
                ('n',  'not', False, None, """reverses the meaning of the "addon" option.  If the "addon" option has a list, then specify everything except what's on the list"""),
                ('M',  'migrateMetaData', False, None, 'if present, this switch causes the metadata tables to be migrated'),
                ('A',  'migrateAddons', False, None, 'if present, this switch causes the addons in the "addons" option list to be migrated'),
                ('v',  'verbose', False, None, 'print status information as it runs'),
                (None, 'clear', False, None, 'clear all exisiting information from the new database'),
                (None, 'clearAddons', False, None, 'clear all exisiting addons in the "addons" option list information from the new database'),
              ]
    
    workingEnvironment = cse.ConfigurationManager.ConfigurationManager(options)
    workingEnvironment["version"] = version
    
    if workingEnvironment.has_key('help'):
      print >>sys.stderr, "m1 %s\nThis routine migrates data from the old AMO database schems to the new one." % version
      workingEnvironment.outputCommandSummary(sys.stderr, 1)
      sys.exit()
    
  except cse.ConfigurationManager.ConfigurationManagerNotAnOption, x:
      print >>sys.stderr, "m1 %s\n%s\nFor usage, try --help" % (version, x)
      sys.exit()
      
  try:
    if workingEnvironment.has_key("verbose"): 
      print "%s beginning migration version %s with options:" % (mx.DateTime.now(), version)
      workingEnvironment.output()

    oldDatabase = cse.MySQLDatabase.MySQLDatabase(workingEnvironment["oldDatabaseName"], workingEnvironment["oldServerName"], 
                                                  workingEnvironment["oldUserName"], workingEnvironment["oldPassword"])
    newDatabase = cse.MySQLDatabase.MySQLDatabase(workingEnvironment["newDatabaseName"], workingEnvironment["newServerName"], 
                                                  workingEnvironment["newUserName"], workingEnvironment["newPassword"])

    setupAddonSelectionTable(oldDatabase, newDatabase, workingEnvironment)
    
    try:
      if workingEnvironment.has_key("clear"):
        originalAddonsOption = workingEnvironment["addons"]
        workingEnvironment["addons"] = "all"
        cleanAddonsTables(newDatabase, workingEnvironment)
        cleanMetaDataTables(newDatabase, workingEnvironment)
        workingEnvironment["addons"] = originalAddonsOption
      elif workingEnvironment.has_key("clearAddons"):
        cleanAddonsTables(newDatabase, workingEnvironment)
  
      if workingEnvironment.has_key("migrateMetaData"):
        if workingEnvironment.has_key("verbose"): print "%s beginning metadata migration" % mx.DateTime.now()
        cleanMetaDataTables (newDatabase, workingEnvironment)
        applicationsToApplications (oldDatabase, newDatabase, workingEnvironment)
        categoriesToTags (oldDatabase, newDatabase, workingEnvironment)
        osToPlatforms (oldDatabase, newDatabase, workingEnvironment)
        userprofilesToUsers (oldDatabase, newDatabase, workingEnvironment)
        
      if workingEnvironment.has_key("migrateAddons"):
        if workingEnvironment.has_key("verbose"): print "%s beginning addons migration" % mx.DateTime.now()
        cleanAddonsTables (newDatabase, workingEnvironment)
        mainToAddOns (oldDatabase, newDatabase, workingEnvironment)
        authorxrefToAddons_users (oldDatabase, newDatabase, workingEnvironment)
        versionToVerions (oldDatabase, newDatabase, workingEnvironment)
        categoryxrefToAddons_tags (oldDatabase, newDatabase, workingEnvironment)
        previewsToPreviews (oldDatabase, newDatabase, workingEnvironment)
    finally:
      cleanupAddonSelectionTable (oldDatabase, newDatabase, workingEnvironment)
        
  except KeyboardInterrupt:
    print >>sys.stderr, "Interrupted..."
    pass
    
  except Exception, x:
    print >>sys.stderr, x
    traceback.print_exc(file=sys.stderr)
  
  if workingEnvironment.has_key("verbose"): print "done."
