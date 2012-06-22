#!/usr/bin/env python

import stringmanipulation
import filemanagement
import p4commands
import sys

name_space_to_ignore = 'GIPS::'
#only allow one prefix to be removed since allowing multiple will complicate
# things
prefix_to_filter = 'gips'
#words_to_filter = ['Module']
# it might be dangerous to remove GIPS but keep it default
words_to_filter = ['Module','GIPS']

# This script finds all the words that should be replaced in an h-file. Once
# all words that should be replaced are found it does a global search and
# replace.

extensions_to_edit = ['.cpp','.cc','.h']

#line = '    ~hiGIPSCriticalSectionScoped()'
#print line
#position = stringmanipulation.getword(line,11)
#old_word = line[position[0]:position[0]+position[1]]
#result = stringmanipulation.removealloccurances(old_word,'gips')
#new_word = result
#print old_word
#print position[0]
#print position[0]+position[1]
#print new_word
#quit()

# Ignore whole line if any item in this table is a substring of the line
do_not_replace_line_table = []
do_not_replace_line_table.append('namespace GIPS')

# [old_string,new_string]
# List of things to remove that are static:
manual_replace_table = []
#manual_replace_table.append(['using namespace GIPS;',''])
#manual_replace_table.append(['CreateGipsEvent','CreateEvent'])
#manual_replace_table.append(['CreateGIPSTrace','CreateTrace'])
#manual_replace_table.append(['ReturnGIPSTrace','ReturnTrace'])
#manual_replace_table.append(['CreateGIPSFile','CreateFile'])
replace_table = manual_replace_table
#replace_table.append(['GIPS::','webrtc::'])
# List of things to not remove that are static, i.e. exceptions:
# don't replace any of the GIPS_Words since that will affect all files
# do that in a separate script!
do_not_replace_table = []
do_not_replace_table.append('GIPS_CipherTypes')
do_not_replace_table.append('GIPS_AuthenticationTypes')
do_not_replace_table.append('GIPS_SecurityLevels')
do_not_replace_table.append('GIPS_encryption')
do_not_replace_table.append('~GIPS_encryption')
do_not_replace_table.append('GIPS_transport')
do_not_replace_table.append('~GIPS_transport')
do_not_replace_table.append('GIPSTraceCallback')
do_not_replace_table.append('~GIPSTraceCallback')
do_not_replace_table.append('GIPS_RTP_CSRC_SIZE')
do_not_replace_table.append('GIPS_RTPDirections')
do_not_replace_table.append('GIPS_RTP_INCOMING')
do_not_replace_table.append('GIPS_RTP_OUTGOING')
do_not_replace_table.append('GIPSFrameType')
do_not_replace_table.append('GIPS_FRAME_EMPTY')
do_not_replace_table.append('GIPS_AUDIO_FRAME_SPEECH')
do_not_replace_table.append('GIPS_AUDIO_FRAME_CN')
do_not_replace_table.append('GIPS_VIDEO_FRAME_KEY')
do_not_replace_table.append('GIPS_VIDEO_FRAME_DELTA')
do_not_replace_table.append('GIPS_VIDEO_FRAME_GOLDEN')
do_not_replace_table.append('GIPS_VIDEO_FRAME_DELTA_KEY')
do_not_replace_table.append('GIPS_PacketType')
do_not_replace_table.append('GIPS_PACKET_TYPE_RTP')
do_not_replace_table.append('GIPS_PACKET_TYPE_KEEP_ALIVE')
do_not_replace_table.append('GIPS_AudioLayers')
do_not_replace_table.append('GIPS_AUDIO_PLATFORM_DEFAULT')
do_not_replace_table.append('GIPS_AUDIO_WINDOWS_WAVE')
do_not_replace_table.append('GIPS_AUDIO_WINDOWS_CORE')
do_not_replace_table.append('GIPS_AUDIO_LINUX_ALSA')
do_not_replace_table.append('GIPS_AUDIO_LINUX_PULSE')
do_not_replace_table.append('GIPS_AUDIO_FORMAT')
do_not_replace_table.append('GIPS_PCM_16_16KHZ')
do_not_replace_table.append('GIPS_PCM_16_8KHZ')
do_not_replace_table.append('GIPS_G729')
do_not_replace_table.append('GIPSAMRmode')
do_not_replace_table.append('GIPS_RFC3267_BWEFFICIENT')
do_not_replace_table.append('GIPS_RFC3267_OCTETALIGNED')
do_not_replace_table.append('GIPS_RFC3267_FILESTORAGE')
do_not_replace_table.append('GIPS_NCModes')
do_not_replace_table.append('GIPS_NC_OFF')
do_not_replace_table.append('GIPS_NC_MILD')
do_not_replace_table.append('GIPS_NC_MODERATE')
do_not_replace_table.append('GIPS_NC_AGGRESSIVE')
do_not_replace_table.append('GIPS_NC_VERY_AGGRESSIVE')
do_not_replace_table.append('GIPS_AGCModes')
do_not_replace_table.append('GIPS_AGC_OFF')
do_not_replace_table.append('GIPS_AGC_ANALOG')
do_not_replace_table.append('GIPS_AGC_DIGITAL')
do_not_replace_table.append('GIPS_AGC_STANDALONE_DIG')
do_not_replace_table.append('GIPS_ECModes')
do_not_replace_table.append('GIPS_EC_UNCHANGED')
do_not_replace_table.append('GIPS_EC_DEFAULT')
do_not_replace_table.append('GIPS_EC_CONFERENCE')
do_not_replace_table.append('GIPS_EC_AEC')
do_not_replace_table.append('GIPS_EC_AES')
do_not_replace_table.append('GIPS_EC_AECM')
do_not_replace_table.append('GIPS_EC_NEC_IAD')
do_not_replace_table.append('GIPS_AESModes')
do_not_replace_table.append('GIPS_AES_DEFAULT')
do_not_replace_table.append('GIPS_AES_NORMAL')
do_not_replace_table.append('GIPS_AES_HIGH')
do_not_replace_table.append('GIPS_AES_ATTENUATE')
do_not_replace_table.append('GIPS_AES_NORMAL_SOFT_TRANS')
do_not_replace_table.append('GIPS_AES_HIGH_SOFT_TRANS')
do_not_replace_table.append('GIPS_AES_ATTENUATE_SOFT_TRANS')
do_not_replace_table.append('GIPS_AECMModes')
do_not_replace_table.append('GIPS_AECM_QUIET_EARPIECE_OR_HEADSET')
do_not_replace_table.append('GIPS_AECM_EARPIECE')
do_not_replace_table.append('GIPS_AECM_LOUD_EARPIECE')
do_not_replace_table.append('GIPS_AECM_SPEAKERPHONE')
do_not_replace_table.append('GIPS_AECM_LOUD_SPEAKERPHONE')
do_not_replace_table.append('AECM_LOUD_SPEAKERPHONE')
do_not_replace_table.append('GIPS_VAD_CONVENTIONAL')
do_not_replace_table.append('GIPS_VAD_AGGRESSIVE_LOW')
do_not_replace_table.append('GIPS_VAD_AGGRESSIVE_MID')
do_not_replace_table.append('GIPS_VAD_AGGRESSIVE_HIGH')
do_not_replace_table.append('GIPS_NetEQModes')
do_not_replace_table.append('GIPS_NETEQ_DEFAULT')
do_not_replace_table.append('GIPS_NETEQ_STREAMING')
do_not_replace_table.append('GIPS_NETEQ_FAX')
do_not_replace_table.append('GIPS_NetEQBGNModes')
do_not_replace_table.append('GIPS_BGN_ON')
do_not_replace_table.append('GIPS_BGN_FADE')
do_not_replace_table.append('GIPS_BGN_OFF')
do_not_replace_table.append('GIPS_OnHoldModes')
do_not_replace_table.append('GIPS_HOLD_SEND_AND_PLAY')
do_not_replace_table.append('GIPS_HOLD_SEND_ONLY')
do_not_replace_table.append('GIPS_HOLD_PLAY_ONLY')
do_not_replace_table.append('GIPS_PayloadFrequencies')
do_not_replace_table.append('GIPS_FREQ_8000_HZ')
do_not_replace_table.append('GIPS_FREQ_16000_HZ')
do_not_replace_table.append('GIPS_FREQ_32000_HZ')
do_not_replace_table.append('GIPS_TelephoneEventDetectionMethods')
do_not_replace_table.append('GIPS_IN_BAND')
do_not_replace_table.append('GIPS_OUT_OF_BAND')
do_not_replace_table.append('GIPS_IN_AND_OUT_OF_BAND')
do_not_replace_table.append('GIPS_ProcessingTypes')
do_not_replace_table.append('GIPS_PLAYBACK_PER_CHANNEL')
do_not_replace_table.append('GIPS_PLAYBACK_ALL_CHANNELS_MIXED')
do_not_replace_table.append('GIPS_RECORDING_PER_CHANNEL')
do_not_replace_table.append('GIPS_RECORDING_ALL_CHANNELS_MIXED')
do_not_replace_table.append('GIPS_StereoChannel')
do_not_replace_table.append('GIPS_StereoLeft')
do_not_replace_table.append('GIPS_StereoRight')
do_not_replace_table.append('GIPS_StereoBoth')
do_not_replace_table.append('GIPS_stat_val')
do_not_replace_table.append('GIPS_P56_statistics')
do_not_replace_table.append('GIPS_echo_statistics')
do_not_replace_table.append('GIPS_NetworkStatistics')
do_not_replace_table.append('GIPS_JitterStatistics')
do_not_replace_table.append('GIPSVideoRawType')
do_not_replace_table.append('GIPS_VIDEO_I420')
do_not_replace_table.append('GIPS_VIDEO_YV12')
do_not_replace_table.append('GIPS_VIDEO_YUY2')
do_not_replace_table.append('GIPS_VIDEO_UYVY')
do_not_replace_table.append('GIPS_VIDEO_IYUV')
do_not_replace_table.append('GIPS_VIDEO_ARGB')
do_not_replace_table.append('GIPS_VIDEO_RGB24')
do_not_replace_table.append('GIPS_VIDEO_RGB565')
do_not_replace_table.append('GIPS_VIDEO_ARGB4444')
do_not_replace_table.append('GIPS_VIDEO_ARGB1555')
do_not_replace_table.append('GIPS_VIDEO_MJPG')
do_not_replace_table.append('GIPS_VIDEO_NV12')
do_not_replace_table.append('GIPS_VIDEO_NV21')
do_not_replace_table.append('GIPS_VIDEO_Unknown')
do_not_replace_table.append('GIPSVideoLayouts')
do_not_replace_table.append('GIPS_LAYOUT_NONE')
do_not_replace_table.append('GIPS_LAYOUT_DEFAULT')
do_not_replace_table.append('GIPS_LAYOUT_ADVANCED1')
do_not_replace_table.append('GIPS_LAYOUT_ADVANCED2')
do_not_replace_table.append('GIPS_LAYOUT_ADVANCED3')
do_not_replace_table.append('GIPS_LAYOUT_ADVANCED4')
do_not_replace_table.append('GIPS_LAYOUT_FULL')
do_not_replace_table.append('KGIPSConfigParameterSize')
do_not_replace_table.append('KGIPSPayloadNameSize')
do_not_replace_table.append('GIPSVideoCodecH263')
do_not_replace_table.append('GIPSVideoH264Packetization')
do_not_replace_table.append('GIPS_H264_SingleMode')
do_not_replace_table.append('GIPS_H264_NonInterleavedMode')
do_not_replace_table.append('GIPSVideoCodecComplexity')
do_not_replace_table.append('GIPSVideoCodec_Complexity_Normal')
do_not_replace_table.append('GIPSVideoCodec_Comlexity_High')
do_not_replace_table.append('GIPSVideoCodec_Comlexity_Higher')
do_not_replace_table.append('GIPSVideoCodec_Comlexity_Max')
do_not_replace_table.append('GIPSVideoCodecH264')
do_not_replace_table.append('GIPSVideoH264Packetization')
do_not_replace_table.append('GIPSVideoCodecComplexity')
do_not_replace_table.append('GIPSVideoCodecProfile')
do_not_replace_table.append('KGIPSConfigParameterSize')
do_not_replace_table.append('KGIPSMaxSVCLayers')
do_not_replace_table.append('GIPSVideoH264LayerTypes')
do_not_replace_table.append('GIPS_H264SVC_Base')
do_not_replace_table.append('GIPS_H264SVC_Extend_2X2')
do_not_replace_table.append('GIPS_H264SVC_Extend_1X1')
do_not_replace_table.append('GIPS_H264SVC_Extend_MGS')
do_not_replace_table.append('GIPS_H264SVC_Extend_1_5')
do_not_replace_table.append('GIPS_H264SVC_Extend_Custom')
do_not_replace_table.append('GIPSVideoH264LayersProperties')
do_not_replace_table.append('GIPSVideoH264LayerTypes')
do_not_replace_table.append('GIPSVideoH264Layers')
do_not_replace_table.append('GIPSVideoH264LayersProperties')
do_not_replace_table.append('GIPSVideoCodecH264SVC')
do_not_replace_table.append('GIPSVideoCodecComplexity')
do_not_replace_table.append('GIPSVideoCodecProfile')
do_not_replace_table.append('GIPSVideoH264Layers')
do_not_replace_table.append('GIPSVideoCodecVP8')
do_not_replace_table.append('GIPSVideoCodecComplexity')
do_not_replace_table.append('GIPSVideoCodecMPEG')
do_not_replace_table.append('GIPSVideoCodecGeneric')
do_not_replace_table.append('GIPSVideoCodecType')
do_not_replace_table.append('GIPSVideoCodec_H263')
do_not_replace_table.append('GIPSVideoCodec_H264')
do_not_replace_table.append('GIPSVideoCodec_H264SVC')
do_not_replace_table.append('GIPSVideoCodec_VP8')
do_not_replace_table.append('GIPSVideoCodec_MPEG4')
do_not_replace_table.append('GIPSVideoCodec_I420')
do_not_replace_table.append('GIPSVideoCodec_RED')
do_not_replace_table.append('GIPSVideoCodec_ULPFEC')
do_not_replace_table.append('GIPSVideoCodec_Unknown')
do_not_replace_table.append('GIPSVideoCodecUnion')
do_not_replace_table.append('GIPSVideoCodecH263')
do_not_replace_table.append('GIPSVideoCodecH264')
do_not_replace_table.append('GIPSVideoCodecH264SVC')
do_not_replace_table.append('GIPSVideoCodecVP8')
do_not_replace_table.append('GIPSVideoCodecMPEG4')
do_not_replace_table.append('GIPSVideoCodecGeneric')
do_not_replace_table.append('GIPSVideoCodec')
do_not_replace_table.append('GIPSVideoCodecType')
do_not_replace_table.append('GIPSVideoCodecUnion')
do_not_replace_table.append('GIPSAudioFrame')
do_not_replace_table.append('GIPS_CodecInst')
do_not_replace_table.append('GIPS_FileFormats')
do_not_replace_table.append('GIPSTickTime')
do_not_replace_table.append('GIPS_Word64')
do_not_replace_table.append('GIPS_UWord64')
do_not_replace_table.append('GIPS_Word32')
do_not_replace_table.append('GIPS_UWord32')
do_not_replace_table.append('GIPS_Word16')
do_not_replace_table.append('GIPS_UWord16')
do_not_replace_table.append('GIPS_Word8')
do_not_replace_table.append('GIPS_UWord8')

if((len(sys.argv) != 2) and (len(sys.argv) != 3)):
    print 'parameters are: parent directory [--commit]'
    quit()

if((len(sys.argv) == 3) and (sys.argv[2] != '--commit')):
    print 'parameters are: parent directory [--commit]'
    quit()

commit = (len(sys.argv) == 3)

directory = sys.argv[1];
if(not filemanagement.pathexist(directory)):
    print 'path ' + directory + ' does not exist'
    quit()

# APIs are all in h-files
extension = '.h'

# All h-files
files_to_modify = filemanagement.listallfilesinfolder(directory,\
                                                      extension)

def isinmanualremovetable( compare_word ):
    for old_word, new_word in manual_replace_table:
        if(old_word == compare_word):
            return True
    return False

# Begin
# This function looks at each line and decides which words should be replaced
# that is this is the only part of the script that you will ever want to change!
def findstringstoreplace(line):
    original_line = line
# Dont replace compiler directives
    if(line[0] == '#'):
        return []
# Dont allow global removal of namespace gips since it is very intrusive
    for sub_string_compare in do_not_replace_line_table:
        index = stringmanipulation.issubstring(line,sub_string_compare)
        if(index != -1):
            return []

    return_value = []

    line = stringmanipulation.removeccomment(line)
    line = stringmanipulation.whitespacestoonespace(line)
    if(len(line) == 0):
        return []
    if(line[0] == '*'):
        return []
    index = stringmanipulation.issubstring(line,prefix_to_filter)
    while index >= 0:
        dont_store_hit = False
        word_position = stringmanipulation.getword(line, index)
        start_of_word = word_position[0]
        size_of_word = word_position[1]
        end_of_word = start_of_word + size_of_word
        old_word = line[start_of_word:end_of_word]
        if(isinmanualremovetable(old_word)):
            dont_store_hit = True
        if((end_of_word + 2 < len(line)) and\
           name_space_to_ignore == line[start_of_word:end_of_word+2]):
            dont_store_hit = True

        result = stringmanipulation.removeprefix(old_word,prefix_to_filter)
        new_word = result[1]
        for word_to_filter in words_to_filter:
            new_word = stringmanipulation.removealloccurances(new_word,word_to_filter)
        result = stringmanipulation.removeprefix(new_word,'_')
        new_word = result[1]
        new_word = stringmanipulation.fixabbreviations(new_word)
        new_word = stringmanipulation.removealloccurances(new_word,'_')
        if(not dont_store_hit):
            return_value.append([old_word,new_word])
# remove the word we found from the string so we dont find it again
        line = line[0:start_of_word] + line[end_of_word:len(line)]
        index = stringmanipulation.issubstring(line,'GIPS')

    return return_value
# End

# loop through all files
for path, file_name in files_to_modify:
#    if(file_name != 'GIPSTickUtil.h'):
#        continue
    full_file_name = path + file_name
    file_pointer = open(full_file_name,'r')
#    print file_name
#loop through all lines
    for line in file_pointer:
#        print line
        local_replace_string = findstringstoreplace(line)
        #print local_replace_string
        if(len(local_replace_string) != 0):
            replace_table.extend(local_replace_string)


# we have built our replace table now
replace_table = stringmanipulation.removeduplicates( replace_table )
replace_table = stringmanipulation.ordertablesizefirst( replace_table )
replace_table = stringmanipulation.complement(replace_table,\
                                              do_not_replace_table)

def replaceoriginal( path,my_table ):
    size_of_table = len(my_table)
    for index in range(len(my_table)):
        old_name = my_table[index][0]
        new_name = my_table[index][1]
        filemanagement.replacestringinfolder(path, old_name, new_name,\
                                             ".h")
        print (100*index) / (size_of_table*2)

def replaceall( my_table, extension_list ):
    size_of_table = len(my_table)
    for index in range(len(my_table)):
        old_name = my_table[index][0]
        new_name = my_table[index][1]
        new_name = new_name
        for extension in extensions_to_edit:
            filemanagement.replacestringinallsubfolders(old_name, new_name,
                                                        extension)
        print 100*(size_of_table + index) / (size_of_table*2)


if(commit):
    print 'commiting'
    replace_table = stringmanipulation.removenochange(replace_table)
    p4commands.checkoutallfiles()
    replaceoriginal(directory,replace_table)
    replaceall(replace_table,extensions_to_edit)
    p4commands.revertunchangedfiles()
else:
    for old_name, new_name in replace_table:
        print 'Going to replace [' + old_name + '] with [' + new_name + ']'
