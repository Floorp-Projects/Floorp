#!/usr/local/bin/ruby

# Queries an LDAP server for all email addresses (tested against Exchange 5.5),
# and makes nice bugzilla user entries out of them. Also disables Bugzilla users
# that are not found in LDAP.

# $Id: bugzilla_ldapsync.rb,v 1.1 2003/04/22 04:11:32 justdave%syndicomm.com Exp $

require 'ldap'
require 'dbi'
require 'getoptlong'

opts = GetoptLong.new(
    ['--dbname',        '-d', GetoptLong::OPTIONAL_ARGUMENT],
    ['--dbpassword',    '-p', GetoptLong::OPTIONAL_ARGUMENT],
    ['--dbuser',        '-u',     GetoptLong::OPTIONAL_ARGUMENT],
    ['--dbpassfile',    '-P', GetoptLong::OPTIONAL_ARGUMENT],
    ['--ldaphost',      '-h', GetoptLong::REQUIRED_ARGUMENT],
    ['--ldapbase',      '-b', GetoptLong::OPTIONAL_ARGUMENT],
    ['--ldapquery',     '-q', GetoptLong::OPTIONAL_ARGUMENT],
    ['--maildomain',    '-m', GetoptLong::OPTIONAL_ARGUMENT],
    ['--noremove',      '-n', GetoptLong::OPTIONAL_ARGUMENT],
    ['--defaultpass',   '-D', GetoptLong::OPTIONAL_ARGUMENT],
    ['--checkmode',     '-c', GetoptLong::OPTIONAL_ARGUMENT]
)


# in hash to make it easy
optHash = Hash.new
opts.each do |opt, arg|
	optHash[opt]=arg
end    

# grab password from file if it's an option
if optHash['--dbpassfile']
    dbPassword=File.open(optHash['--dbpassfile'], 'r').readlines[0].chomp!
else
    dbPassword=optHash['--dbpassword'] || nil
end

# make bad assumptions.
dbName = optHash['--dbname'] || 'bugzilla'
dbUser = optHash['--dbuser'] || 'bugzilla'
ldapHost = optHash['--ldaphost'] || 'ldap'
ldapBase = optHash['--ldapbase'] || ''
mailDomain = optHash['--maildomain'] || `domainname`.chomp!
ldapQuery = optHash['--ldapquery'] || "(&(objectclass=person)(rfc822Mailbox=*@#{mailDomain}))"
checkMode = optHash['--checkmode'] || nil
noRemove = optHash['--noremove'] || nil
defaultPass = optHash['--defaultpass'] || 'bugzilla'

if (! dbPassword)
    puts "bugzilla_ldapsync (c) 2003 Thomas Stromberg <thomas+bugzilla@stromberg.org>"
    puts ""
    puts " -d | --dbname        name of MySQL database            [#{dbName}]"
    puts " -u | --dbuser        username for MySQL database       [#{dbUser}]"
    puts " -p | --dbpassword    password for MySQL user           [#{dbPassword}]"
    puts " -P | --dbpassfile    filename containing password for MySQL user"
    puts " -h | --ldaphost      hostname for LDAP server          [#{ldapHost}]"
    puts " -b | --ldapbase      Base of LDAP query, for instance, o=Bugzilla.com"
    puts " -q | --ldapquery     LDAP query, uses maildomain       [#{ldapQuery}]"
    puts " -m | --maildomain    e-mail domain to use records from"
    puts " -n | --noremove      do not remove Bugzilla users that are not in LDAP"
    puts " -c | --checkmode     checkmode, does not perform any SQL changes"
    puts " -D | --defaultpass   default password for new users    [#{defaultPass}]"
    puts
    puts "example:"
    puts
    puts " bugzilla_ldapsync.rb -c -u taskzilla -P /tmp/test -d taskzilla -h bhncmail -m \"bowebellhowell.com\""
    exit
end


if (checkMode)
    puts '(checkmode enabled, no SQL writes will actually happen)'
    puts "ldapquery is #{ldapQuery}"
    puts
end
    

bugzillaUsers = Array.new
ldapUsers = Array.new
encPassword = defaultPass.crypt('xx')
sqlNewUser = "INSERT INTO profiles VALUES ('', ?, '#{encPassword}', ?, '', 1, NULL, '0000-00-00 00:00:00');"

# presumes that the MySQL database is local.
dbh = DBI.connect("DBI:Mysql:#{dbName}", dbUser, dbPassword)

# select all e-mail addresses where there is no disabledtext defined. Only valid users, please!
dbh.select_all('select login_name from profiles WHERE disabledtext = ""') { |row|
	bugzillaUsers.push(row[0].downcase)
}


LDAP::Conn.new(ldapHost, 389).bind{|conn|
  sub = nil
  # perform the query, but only get the e-mail address, location, and name returned to us.
  conn.search(ldapBase, LDAP::LDAP_SCOPE_SUBTREE, ldapQuery,  
  	['rfc822Mailbox', 'physicalDeliveryOfficeName', 'cn'])  { |entry|
	
		# Get the users first (primary) e-mail address, but I only want what's before the @ sign.
		entry.vals("rfc822Mailbox")[0] =~ /([\w\.-]+)\@/
		email = $1
	
		# We put the officename in the users description, and nothing otherwise.
		if entry.vals("physicalDeliveryOfficeName")
			location = entry.vals("physicalDeliveryOfficeName")[0]
		else
			location = ''
		end
	
		# for whatever reason, we get blank entries. Do some double checking here.
		if (email && (email.length > 4) && (location !~ /Generic/) && (entry.vals("cn")))		
			if (location.length > 2) 
				desc = entry.vals("cn")[0] + " (" + location + ")"
			else
				desc = entry.vals("cn")[0]
			end

			# slash out apostrophes. This is not how we are supposed to do it.
			desc.sub!('\'', "\\'")
			email.sub!("\'", "%")
			email.sub!('%', "\'")
            ldapUsers.push(email.downcase)
            
            # while we are in this loop, lets go ahead and add the new users to Bugzilla
			if (! bugzillaUsers.include?(email.downcase)) 
                if (! checkMode) 
    				dbh.do(sqlNewUser, email, desc)
                end
                puts "added #{email} - #{desc}"
			end
		end
	}
}

# This is the loop that takes the users that we found in Bugzilla originally, and 
# checks to see if they are still in the LDAP server. If they are not, away they go!
if (! noRemove)
    bugzillaUsers.each { |user|
        if (! ldapUsers.include?(user))
            if (! checkMode)
                 dbh.do("UPDATE profiles SET disabledtext = \'auto-disabled by sync\' WHERE login_name=\"#{user}\"")
            end
            puts "disabled #{user}"
        end
    }
end
dbh.disconnect

