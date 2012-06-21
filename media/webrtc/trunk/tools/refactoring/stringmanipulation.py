import string

# returns tuple, [success,updated_string] where the updated string has
# has one less (the first) occurance of match string
def removefirstoccurance( remove_string, match_string ):
    lowercase_string = remove_string.lower()
    lowercase_match_string = match_string.lower()
    lowest_index = lowercase_string.find(lowercase_match_string)
    if(lowest_index == -1):
        return [False,remove_string]
    past_match_index = lowest_index + len(lowercase_match_string)
    highest_index = len(remove_string)
    remove_string = remove_string[0:lowest_index] + remove_string[past_match_index: highest_index]
    return [True,remove_string]

# returns a string with all occurances of match_string removed
def removealloccurances( remove_string, match_string ):
    return_value = [True, remove_string]
    while(return_value[0]):
        return_value = removefirstoccurance(return_value[1],match_string)
    return return_value[1]

# removes an occurance of match_string only if it's first in the string
# returns tuple [succes, new_string]
def removeprefix( remove_string, match_string ):
    lowercase_string = remove_string.lower()
    lowercase_match_string = match_string.lower()
    lowest_index = lowercase_string.find(lowercase_match_string)
    if(lowest_index == -1):
        return [False,remove_string]
    if(lowest_index != 0):
        return [False,remove_string]
    past_match_index = lowest_index + len(lowercase_match_string)
    highest_index = len(remove_string)
    remove_string = remove_string[0:lowest_index] + remove_string[past_match_index: highest_index]
#    print lowest_index
#    print past_match_index
    return [True,remove_string]

# removes multiple occurances of match string as long as they are first in
# the string
def removeallprefix( remove_string, match_string ):
    return_value = [True, remove_string]
    while(return_value[0]):
        return_value = removeprefix(return_value[1],match_string)
    return return_value[1]

# returns true if extensionstring is a correct extension
def isextension( extensionstring ):
    if(len(extensionstring) < 2):
        return False
    if(extensionstring[0] != '.'):
        return False
    if(extensionstring[1:len(extensionstring)-1].find('.') != -1):
        return False
    return True

# returns the index of start of the last occurance of match_string
def findlastoccurance( original_string, match_string ):
    search_index = original_string.find(match_string)
    found_index = search_index
    last_index = len(original_string) - 1
    while((search_index != -1) and (search_index < last_index)):
        search_index = original_string[search_index+1:last_index].find(match_string)
        if(search_index != -1):
            found_index = search_index
    return found_index

# changes extension from original_extension to new_extension
def changeextension( original_string, original_extension, new_extension):
    if(not isextension(original_extension)):
        return original_string
    if(not isextension(new_extension)):
        return original_string
    index = findlastoccurance(original_string, original_extension)
    if(index == -1):
        return original_string
    return_value = original_string[0:index] + new_extension
    return return_value

# wanted to do this with str.find however didnt seem to work so do it manually
# returns the index of the first capital letter
def findfirstcapitalletter( original_string ):
    for index in range(len(original_string)):
        if(original_string[index].lower() != original_string[index]):
            return index
    return -1


# replaces capital letters with underscore and lower case letter (except very
# first
def lowercasewithunderscore( original_string ):
# ignore the first letter since there should be no underscore in front of it
    if(len(original_string) < 2):
        return original_string
    return_value = original_string[1:len(original_string)]
    index = findfirstcapitalletter(return_value)
    while(index != -1):
        return_value = return_value[0:index] + \
                       '_' + \
                       return_value[index].lower() + \
                       return_value[index+1:len(return_value)]
        index = findfirstcapitalletter(return_value)
    return_value = original_string[0].lower() + return_value
    return return_value

# my table is a duplicate of strings
def removeduplicates( my_table ):
    new_table = []
    for old_string1, new_string1 in my_table:
        found = 0
        for old_string2, new_string2 in new_table:
            if(old_string1 == old_string2):
                found += 1
            if(new_string1 == new_string2):
                if(new_string1 == ''):
                    found += found
                else:
                    found += 1
            if(found == 1):
                print 'missmatching set, terminating program'
                print old_string1
                print new_string1
                print old_string2
                print new_string2
                quit()
            if(found == 2):
                break
        if(found == 0):
            new_table.append([old_string1,new_string1])
    return new_table

def removenochange( my_table ):
    new_table = []
    for old_string, new_string in my_table:
        if(old_string != new_string):
            new_table.append([old_string,new_string])
    return new_table

# order table after size of the string (can be used to replace bigger strings
# first which is useful since smaller strings can be inside the bigger string)
# E.g. GIPS is a sub string of GIPSVE if we remove GIPS first GIPSVE will never
# be removed. N is small so no need for fancy sort algorithm. Use selection sort
def ordertablesizefirst( my_table ):
    for current_index in range(len(my_table)):
        biggest_string = 0
        biggest_string_index = -1
        for search_index in range(len(my_table)):
            if(search_index < current_index):
                continue
            length_of_string = len(my_table[search_index][0])
            if(length_of_string > biggest_string):
                biggest_string = length_of_string
                biggest_string_index = search_index
        if(biggest_string_index == -1):
            print 'sorting algorithm failed, program exit'
            quit()
        old_value = my_table[current_index]
        my_table[current_index] = my_table[biggest_string_index]
        my_table[biggest_string_index] = old_value
    return my_table

# returns true if string 1 or 2 is a substring of the other, assuming neither
# has whitespaces
def issubstring( string1, string2 ):
    if(len(string1) == 0):
        return -1
    if(len(string2) == 0):
        return -1
    large_string = string1
    small_string = string2
    if(len(string1) < len(string2)):
        large_string = string2
        small_string = string1

    for index in range(len(large_string)):
        large_sub_string = large_string[index:index+len(small_string)].lower()
        if(large_sub_string ==\
           small_string.lower()):
              return index
    return -1

#not_part_of_word_table = [' ','(',')','{','}',':','\t','*','&','/','[',']','.',',','\n']
#def ispartofword( char ):
#    for item in not_part_of_word_table:
#        if(char == item):
#            return False
#    return True

# must be numerical,_ or charachter
def ispartofword( char ):
    if(char.isalpha()):
        return True
    if(char.isalnum()):
        return True
    if(char == '_'):
        return True
    return False

# returns the index of the first letter in the word that the current_index
# is pointing to and the size of the word
def getword( line, current_index):
    if(current_index < 0):
        return []
    line = line.rstrip()
    if(len(line) <= current_index):
        return []
    if(line[current_index] == ' '):
        return []
    start_pos = current_index
    while start_pos >= 0:
        if(not ispartofword(line[start_pos])):
            start_pos += 1
            break
        start_pos -= 1
    if(start_pos == -1):
        start_pos = 0
    end_pos = current_index
    while end_pos < len(line):
        if(not ispartofword(line[end_pos])):
            break
        end_pos += 1
    return [start_pos,end_pos - start_pos]

# my table is a tuple [string1,string2] complement_to_table is just a list
# of strings to compare to string1
def complement( my_table, complement_to_table ):
    new_table = []
    for index in range(len(my_table)):
        found = False;
        for compare_string in complement_to_table:
            if(my_table[index][0].lower() == compare_string.lower()):
                found = True
        if(not found):
            new_table.append(my_table[index])
    return new_table

def removestringfromhead( line, remove_string):
    for index in range(len(line)):
        if(line[index:index+len(remove_string)] != remove_string):
            return line[index:index+len(line)]
    return ''

def removeccomment( line ):
    comment_string = '//'
    for index in range(len(line)):
        if(line[index:index+len(comment_string)] == comment_string):
            return line[0:index]
    return line

def whitespacestoonespace( line ):
    return ' '.join(line.split())

def fixabbreviations( original_string ):
    previouswascapital = (original_string[0].upper() == original_string[0])
    new_string = ''
    for index in range(len(original_string)):
        if(index == 0):
            new_string += original_string[index]
            continue
        if(original_string[index] == '_'):
            new_string += original_string[index]
            previouswascapital = False
            continue
        if(original_string[index].isdigit()):
            new_string += original_string[index]
            previouswascapital = False
            continue
        currentiscapital = (original_string[index].upper() == original_string[index])
        letter_to_add = original_string[index]
        if(previouswascapital and currentiscapital):
            letter_to_add = letter_to_add.lower()
        if(previouswascapital and (not currentiscapital)):
            old_letter = new_string[len(new_string)-1]
            new_string = new_string[0:len(new_string)-1]
            new_string += old_letter.upper()
        previouswascapital = currentiscapital
        new_string += letter_to_add
    return new_string

def replaceoccurances(old_string, replace_string, replace_with_string):
    if (len(replace_string) == 0):
        return old_string
    if (len(old_string) < len(replace_string)):
        return old_string
    # Simple implementation, could proably be done smarter
    new_string = ''
    for index in range(len(old_string)):
        #print new_string
        if(len(replace_string) > (len(old_string) - index)):
            new_string += old_string[index:index + len(old_string)]
            break
        match = (len(replace_string) > 0)
        for replace_index in range(len(replace_string)):
            if (replace_string[replace_index] != old_string[index + replace_index]):
                match = False
                break
        if (match):
            new_string += replace_with_string
            index =+ len(replace_string)
        else:
            new_string += old_string[index]
    return new_string
