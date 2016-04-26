/*
 * mpi-test.c
 *
 * This is a general test suite for the MPI library, which tests
 * all the functions in the library with known values.  The program
 * exits with a zero (successful) status if the tests pass, or a 
 * nonzero status if the tests fail.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <time.h>

#include "mpi.h"
#include "mpprime.h"

#include "test-info.c"

/* ZS means Zero Suppressed (no leading zeros) */
#if MP_USE_LONG_DIGIT 
#define ZS_DIGIT_FMT         "%lX"
#elif MP_USE_LONG_LONG_DIGIT
#define ZS_DIGIT_FMT         "%llX"
#elif MP_USE_UINT_DIGIT 
#define ZS_DIGIT_FMT         "%X"
#else
#error "unknown type of digit"
#endif

/*
  Test vectors

  If you intend to change any of these values, you must also recompute
  the corresponding solutions below.  Basically, these are just hex
  strings (for the big integers) or integer values (for the digits).

  The comparison tests think they know what relationships hold between
  these values.  If you change that, you may have to adjust the code
  for the comparison tests accordingly.  Most of the other tests
  should be fine as long as you re-compute the solutions, though.
 */
const char   *mp1  = "639A868CDA0C569861B";
const char   *mp2  = "AAFC0A3FE45E5E09DBE2C29";
const char   *mp3  = "B55AA8DF8A7E83241F38AC7A9E479CAEF2E4D7C5";
const char   *mp4  = "-63DBC2265B88268DC801C10EA68476B7BDE0090F";
const char   *mp5  = "F595CB42";
const char   *mp5a = "-4B597E";
const char   *mp6  = "0";
const char   *mp7  = "EBFA7121CD838CE6439CC59DDB4CBEF3";
const char   *mp8  = "5";
const char   *mp9  = "F74A2876A1432698923B0767DA19DCF3D71795EE";
const char   *mp10 = "9184E72A000";
const char   *mp11 = "54D79A3557E8";
const char   *mp12 = "10000000000000000";
const char   *mp13 = 
"34584F700C15A341E40BF7BFDD88A6630C8FF2B2067469372D391342BDAB6163963C"
"D5A5C79F708BDE26E0CCF2DB66CD6D6089E29A877C45F2B050D226E6DA88";
const char   *mp14 =
"AC3FA0EABAAC45724814D798942A1E28E14C81E0DE8055CED630E7689DA648683645DB6E"
"458D9F5338CC3D4E33A5D1C9BF42780133599E60DEE0049AFA8F9489501AE5C9AA2B8C13"
"FD21285A538B2CA87A626BB56E0A654C8707535E637FF4E39174157402BDE3AA30C9F134"
"0C1307BAA864B075A9CC828B6A5E2B2BF1AE406D920CC5E7657D7C0E697DEE5375773AF9"
"E200A1B8FAD7CD141F9EE47ABB55511FEB9A4D99EBA22F3A3FF6792FA7EE9E5DC0EE94F7"
"7A631EDF3D7DD7C2DAAAFDF234D60302AB63D5234CEAE941B9AF0ADDD9E6E3A940A94EE5"
"5DB45A7C66E61EDD0477419BBEFA44C325129601C4F45671C6A0E64665DF341D17FBC71F"
"77418BD9F4375DDB3B9D56126526D8E5E0F35A121FD4F347013DA880020A752324F31DDD"
"9BCDB13A3B86E207A2DE086825E6EEB87B3A64232CFD8205B799BC018634AAE193F19531"
"D6EBC19A75F27CFFAA03EB5974898F53FD569AA5CE60F431B53B0CDE715A5F382405C9C4"
"761A8E24888328F09F7BCE4E8D80C957DF177629C8421ACCD0C268C63C0DD47C3C0D954F"
"D79F7D7297C6788DF4B3E51381759864D880ACA246DF09533739B8BB6085EAF7AE8DC2D9"
"F224E6874926C8D24D34B457FD2C9A586C6B99582DC24F787A39E3942786CF1D494B6EB4"
"A513498CDA0B217C4E80BCE7DA1C704C35E071AC21E0DA9F57C27C3533F46A8D20B04137"
"C1B1384BE4B2EB46";
const char   *mp15 = 
"39849CF7FD65AF2E3C4D87FE5526221103D90BA26A6642FFE3C3ECC0887BBBC57E011BF1"
"05D822A841653509C68F79EBE51C0099B8CBB04DEF31F36F5954208A3209AC122F0E11D8"
"4AE67A494D78336A2066D394D42E27EF6B03DDAF6D69F5112C93E714D27C94F82FC7EF77"
"445768C68EAE1C4A1407BE1B303243391D325090449764AE469CC53EC8012C4C02A72F37"
"07ED7275D2CC8D0A14B5BCC6BF264941520EBA97E3E6BAE4EE8BC87EE0DDA1F5611A6ECB"
"65F8AEF4F184E10CADBDFA5A2FEF828901D18C20785E5CC63473D638762DA80625003711"
"9E984AC43E707915B133543AF9D5522C3E7180DC58E1E5381C1FB7DC6A5F4198F3E88FA6"
"CBB6DFA8B2D1C763226B253E18BCCB79A29EE82D2DE735078C8AE3C3C86D476AAA08434C"
"09C274BDD40A1D8FDE38D6536C22F44E807EB73DE4FB36C9F51E0BC835DDBE3A8EFCF2FE"
"672B525769DC39230EE624D5EEDBD837C82A52E153F37378C3AD68A81A7ADBDF3345DBCE"
"8FA18CA1DE618EF94DF72EAD928D4F45B9E51632ACF158CF8332C51891D1D12C2A7E6684"
"360C4BF177C952579A9F442CFFEC8DAE4821A8E7A31C4861D8464CA9116C60866C5E72F7"
"434ADBED36D54ACDFDFF70A4EFB46E285131FE725F1C637D1C62115EDAD01C4189716327"
"BFAA79618B1656CBFA22C2C965687D0381CC2FE0245913C4D8D96108213680BD8E93E821"
"822AD9DDBFE4BD04";
const char   *mp16 = "4A724340668DB150339A70";
const char   *mp17 = "8ADB90F58";
const char   *mp18 = "C64C230AB20E5";
const char *mp19 = 
"F1C9DACDA287F2E3C88DCE2393B8F53DAAAC1196DC36510962B6B59454CFE64B";
const char *mp20 = 
"D445662C8B6FE394107B867797750C326E0F4A967E135FC430F6CD7207913AC7";
const char* mp21 = "2";

const mp_digit md1 = 0;
const mp_digit md2 = 0x1;
const mp_digit md3 = 0x80;
const mp_digit md4 = 0x9C97;
const mp_digit md5 = 0xF5BF;
const mp_digit md6 = 0x14A0;
const mp_digit md7 = 0x03E8;
const mp_digit md8 = 0x0101;
const mp_digit md9 = 0xA;

/* 
   Solutions of the form x_mpABC, where:

   x = (p)roduct, (s)um, (d)ifference, (q)uotient, (r)emainder, (g)cd,
       (i)nverse, (e)xponent, square roo(t), (g)cd, (l)cm.  A
       leading 'm' indicates a modular operation, e.g. ms_mp12 is the
       modular sum of operands 1 and 2

   ABC are the operand numbers involved in the computation.  If a 'd'
   precedes the number, it is a digit operand; if a 'c' precedes it,
   it is a constant; otherwise, it is a full integer.  
 */

const char *p_mp12   = "4286AD72E095C9FE009938750743174ADDD7FD1E53";
const char *p_mp34   = "-46BDBD66CA108C94A8CF46C325F7B6E2F2BA82D35"
                       "A1BFD6934C441EE369B60CA29BADC26845E918B";
const char *p_mp57   = "E260C265A0A27C17AD5F4E59D6E0360217A2EBA6";
const char *p_mp22   = "7233B5C1097FFC77CCF55928FDC3A5D31B712FDE7A1E91";
const char *p_mp1d4  = "3CECEA2331F4220BEF68DED";
const char *p_mp8d6  = "6720";
const char *p_mp1113 =
"11590FC3831C8C3C51813142C88E566408DB04F9E27642F6471A1822E0100B12F7F1"
"5699A127C0FA9D26DCBFF458522661F30C6ADA4A07C8C90F9116893F6DBFBF24C3A2"
"4340";
const char *p_mp1415 = 
"26B36540DE8B3586699CCEAE218A2842C7D5A01590E70C4A26E789107FBCDB06AA2C"
"6DDC39E6FA18B16FCB2E934C9A5F844DAD60EE3B1EA82199EC5E9608F67F860FB965"
"736055DF0E8F2540EB28D07F47E309B5F5D7C94FF190AB9C83A6970160CA700B1081"
"F60518132AF28C6CEE6B7C473E461ABAC52C39CED50A08DD4E7EA8BA18DAD545126D"
"A388F6983C29B6BE3F9DCBC15766E8E6D626A92C5296A9C4653CAE5788350C0E2107"
"F57E5E8B6994C4847D727FF1A63A66A6CEF42B9C9E6BD04C92550B85D5527DE8A132"
"E6BE89341A9285C7CE7FB929D871BBCBD0ED2863B6B078B0DBB30FCA66D6C64284D6"
"57F394A0271E15B6EC7A9D530EBAC6CA262EF6F97E1A29FCE7749240E4AECA591ECF"
"272122BC587370F9371B67BB696B3CDC1BC8C5B64B6280994EBA00CDEB8EB0F5D06E"
"18F401D65FDCECF23DD7B9BB5B4C5458AEF2CCC09BA7F70EACB844750ACFD027521E"
"2E047DE8388B35F8512D3DA46FF1A12D4260213602BF7BFFDB6059439B1BD0676449"
"8D98C74F48FB3F548948D5BA0C8ECFCD054465132DC43466D6BBD59FBAF8D6D4E157"
"2D612B40A956C7D3E140F3B8562EF18568B24D335707D5BAC7495014DF2444172426"
"FD099DED560D30D1F945386604AFC85C64BD1E5F531F5C7840475FC0CF0F79810012"
"4572BAF5A9910CDBD02B27FFCC3C7E5E88EF59F3AE152476E33EDA696A4F751E0AE4"
"A3D2792DEA78E25B9110E12A19EFD09EA47FF9D6594DA445478BEB6901EAF8A35B2D"
"FD59BEE9BF7AA8535B7D326EFA5AA2121B5EBE04DD85827A3D43BD04F4AA6D7B62A2"
"B6D7A3077286A511A431E1EF75FCEBA3FAE9D5843A8ED17AA02BBB1B571F904699C5"
"A6073F87DDD012E2322AB3F41F2A61F428636FE86914148E19B8EF8314ED83332F2F"
"8C2ADE95071E792C0A68B903E060DD322A75FD0C2B992059FCCBB58AFA06B50D1634"
"BBD93F187FCE0566609FCC2BABB269C66CEB097598AA17957BB4FDA3E64A1B30402E"
"851CF9208E33D52E459A92C63FBB66435BB018E155E2C7F055E0B7AB82CD58FC4889"
"372ED9EEAC2A07E8E654AB445B9298D2830D6D4DFD117B9C8ABE3968927DC24B3633"
"BAD6E6466DB45DDAE87A0AB00336AC2CCCE176704F7214FCAB55743AB76C2B6CA231"
"7984610B27B5786DE55C184DDF556EDFEA79A3652831940DAD941E243F482DC17E50"
"284BC2FB1AD712A92542C573E55678878F02DFD9E3A863C7DF863227AEDE14B47AD3"
"957190124820ADC19F5353878EDB6BF7D0C77352A6E3BDB53EEB88F5AEF6226D6E68"
"756776A8FB49B77564147A641664C2A54F7E5B680CCC6A4D22D894E464DF20537094"
"548F1732452F9E7F810C0B4B430C073C0FBCE03F0D03F82630654BCE166AA772E1EE"
"DD0C08D3E3EBDF0AF54203B43AFDFC40D8FC79C97A4B0A4E1BEB14D8FCEFDDED8758"
"6ED65B18";
const char *p_mp2121 = "4";
const char *mp_mp345 = "B9B6D3A3";
const char *mp_mp335 = "16609C2D";

const char *s_mp13   = "B55AA8DF8A7E83241F38B2B446B06A4FB84E5DE0";
const char *s_mp34   = "517EE6B92EF65C965736EB6BF7C325F73504CEB6";
const char *s_mp46   = "-63DBC2265B88268DC801C10EA68476B7BDE0090F";
const char *s_mp5d4  = "F59667D9";
const char *s_mp2d5  = "AAFC0A3FE45E5E09DBF21E8";
const char *s_mp1415 = 
"E5C43DE2B811F4A084625F96E9504039E5258D8348E698CEB9F4D4292622042DB446"
"F75F4B65C1FB7A317257FA354BB5A45E789AEC254EAECE11F80A53E3B513822491DB"
"D9399DEC4807A2A3A10360129AC93F4A42388D3BF20B310DD0E9E9F4BE07FC88D53A"
"78A26091E0AB506A70813712CCBFBDD440A69A906E650EE090FDD6A42A95AC1A414D"
"317F1A9F781E6A30E9EE142ECDA45A1E3454A1417A7B9A613DA90831CF88EA1F2E82"
"41AE88CC4053220903C2E05BCDD42F02B8CF8868F84C64C5858BAD356143C5494607"
"EE22E11650148BAF65A985F6FC4CA540A55697F2B5AA95D6B8CF96EF638416DE1DD6"
"3BA9E2C09E22D03E75B60BE456C642F86B82A709253E5E087B507DE3A45F8392423F"
"4DBC284E8DC88C43CA77BC8DCEFB6129A59025F80F90FF978116DEBB9209E306FBB9"
"1B6111F8B8CFACB7C7C9BC12691C22EE88303E1713F1DFCEB622B8EA102F6365678B"
"C580ED87225467AA78E875868BD53B17574BA59305BC1AC666E4B7E9ED72FCFC200E"
"189D98FC8C5C7533739C53F52DDECDDFA5A8668BFBD40DABC9640F8FCAE58F532940"
"8162261320A25589E9FB51B50F80056471F24B7E1AEC35D1356FC2747FFC13A04B34"
"24FCECE10880BD9D97CA8CDEB2F5969BF4F30256EB5ED2BCD1DC64BDC2EE65217848"
"48A37FB13F84ED4FB7ACA18C4639EE64309BDD3D552AEB4AAF44295943DC1229A497"
"A84A";

const char *ms_mp345 = "1E71E292";

const char *d_mp12   = "-AAFBA6A55DD183FD854A60E";
const char *d_mp34   = "119366B05E606A9B1E73A6D8944CC1366B0C4E0D4";
const char *d_mp5d4  = "F5952EAB";
const char *d_mp6d2  = "-1";
const char *md_mp345 = "26596B86";

const char *q_mp42   = "-95825A1FFA1A155D5";
const char *r_mp42   = "-6312E99D7700A3DCB32ADF2";
const char *q_mp45a  = "15344CDA3D841F661D2B61B6EDF7828CE36";
const char *r_mp45a  = "-47C47B";
const char *q_mp7c2  = "75FD3890E6C1C67321CE62CEEDA65F79";
const char *q_mp3d6  = "8CAFD53C272BD6FE8B0847BDC3B539EFAB5C3";
const char *r_mp3d6  = "1E5";
const char *r_mp5d5  = "1257";
const char *r_mp47   = "B3A9018D970281A90FB729A181D95CB8";
const char *q_mp1404 = 
"-1B994D869142D3EF6123A3CBBC3C0114FA071CFCEEF4B7D231D65591D32501AD80F"
"FF49AE4EC80514CC071EF6B42521C2508F4CB2FEAD69A2D2EF3934087DCAF88CC4C4"
"659F1CA8A7F4D36817D802F778F1392337FE36302D6865BF0D4645625DF8BB044E19"
"930635BE2609FAC8D99357D3A9F81F2578DE15A300964188292107DAC980E0A08CD7"
"E938A2135FAD45D50CB1D8C2D4C4E60C27AB98B9FBD7E4DBF752C57D2674520E4BB2"
"7E42324C0EFE84FB3E38CF6950E699E86FD45FE40D428400F2F94EDF7E94FAE10B45"
"89329E1BF61E5A378C7B31C9C6A234F8254D4C24823B84D0BF8D671D8BC9154DFAC9"
"49BD8ACABD6BD32DD4DC587F22C86153CB3954BDF7C2A890D623642492C482CF3E2C"
"776FC019C3BBC61688B485E6FD35D6376089C1E33F880E84C4E51E8ABEACE1B3FB70"
"3EAD0E28D2D44E7F1C0A859C840775E94F8C1369D985A3C5E8114B21D68B3CBB75D2"
"791C586153C85B90CAA483E57A40E2D97950AAB84920A4396C950C87C7FFFE748358"
"42A0BF65445B26D40F05BE164B822CA96321F41D85A289C5F5CD5F438A78704C9683"
"422299D21899A22F853B0C93081CC9925E350132A0717A611DD932A68A0ACC6E4C7F"
"7F685EF8C1F4910AEA5DC00BB5A36FCA07FFEAA490C547F6E14A08FE87041AB803E1"
"BD9E23E4D367A2C35762F209073DFF48F3";
const char *r_mp1404 = "12FF98621ABF63144BFFC3207AC8FC10D8D1A09";

const char *q_mp13c  = 
		"34584F700C15A341E40BF7BFDD88A6630C8FF2B2067469372D391342"
		"BDAB6163963CD5A5C79F708BDE26E0CCF2DB66CD6D6089E29A877C45";
const char *r_mp13c  = "F2B050D226E6DA88";
const char *q_mp9c16 = "F74A2876A1432698923B0767DA19DCF3D71795E";
const char *r_mp9c16 = "E";

const char *e_mp5d9 = "A8FD7145E727A20E52E73D22990D35D158090307A"
		      "13A5215AAC4E9AB1E96BD34E531209E03310400";
const char *e_mp78  = "AA5F72C737DFFD8CCD108008BFE7C79ADC01A819B"
		      "32B75FB82EC0FB8CA83311DA36D4063F1E57857A2"
		      "1AB226563D84A15BB63CE975FF1453BD6750C58D9"
		      "D113175764F5D0B3C89B262D4702F4D9640A3";
const char *me_mp817 = "E504493ACB02F7F802B327AB13BF25";
const char *me_mp5d47 = "1D45ED0D78F2778157992C951DD2734C";
const char *me_mp1512 = "FB5B2A28D902B9D9";
const char *me_mp161718 = "423C6AC6DBD74";
const char *me_mp5114 =
"64F0F72807993578BBA3C7C36FFB184028F9EB9A810C92079E1498D8A80FC848E1F0"
"25F1DE43B7F6AC063F5CC29D8A7C2D7A66269D72BF5CDC327AF88AF8EF9E601DCB0A"
"3F35BFF3525FB1B61CE3A25182F17C0A0633B4089EA15BDC47664A43FEF639748AAC"
"19CF58E83D8FA32CD10661D2D4210CC84792937E6F36CB601851356622E63ADD4BD5"
"542412C2E0C4958E51FD2524AABDC7D60CFB5DB332EEC9DC84210F10FAE0BA2018F2"
"14C9D6867C9D6E49CF28C18D06CE009FD4D04BFC8837C3FAAA773F5CCF6DED1C22DE"
"181786AFE188540586F2D74BF312E595244E6936AE52E45742109BAA76C36F2692F5"
"CEF97AD462B138BE92721194B163254CBAAEE9B9864B21CCDD5375BCAD0D24132724"
"113D3374B4BCF9AA49BA5ACBC12288C0BCF46DCE6CB4A241A91BD559B130B6E9CD3D"
"D7A2C8B280C2A278BA9BF5D93244D563015C9484B86D9FEB602501DC16EEBC3EFF19"
"53D7999682BF1A1E3B2E7B21F4BDCA3C355039FEF55B9C0885F98DC355CA7A6D8ECF"
"5F7F1A6E11A764F2343C823B879B44616B56BF6AE3FA2ACF5483660E618882018E3F"
"C8459313BACFE1F93CECC37B2576A5C0B2714BD3EEDEEC22F0E7E3E77B11396B9B99"
"D683F2447A4004BBD4A57F6A616CDDFEC595C4FC19884CC2FC21CF5BF5B0B81E0F83"
"B9DDA0CF4DFF35BB8D31245912BF4497FD0BD95F0C604E26EA5A8EA4F5EAE870A5BD"
"FE8C";

const char *e_mpc2d3 = "100000000000000000000000000000000";

const char *t_mp9    = "FB9B6E32FF0452A34746";
const char *i_mp27   = "B6AD8DCCDAF92B6FE57D062FFEE3A99";
const char *i_mp2019 = 
"BDF3D88DC373A63EED92903115B03FC8501910AF68297B4C41870AED3EA9F839";
/* "15E3FE09E8AE5523AABA197BD2D16318D3CA148EDF4AE1C1C52FC96AFAF5680B"; */


const char *t_mp15 =
"795853094E59B0008093BCA8DECF68587C64BDCA2F3F7F8963DABC12F1CFFFA9B8C4"
"365232FD4751870A0EF6CA619287C5D8B7F1747D95076AB19645EF309773E9EACEA0"
"975FA4AE16251A8DA5865349C3A903E3B8A2C0DEA3C0720B6020C7FED69AFF62BB72"
"10FAC443F9FFA2950776F949E819260C2AF8D94E8A1431A40F8C23C1973DE5D49AA2"
"0B3FF5DA5C1D5324E712A78FF33A9B1748F83FA529905924A31DF38643B3F693EF9B"
"58D846BB1AEAE4523ECC843FF551C1B300A130B65C1677402778F98C51C10813250E"
"2496882877B069E877B59740DC1226F18A5C0F66F64A5F59A9FAFC5E9FC45AEC0E7A"
"BEE244F7DD3AC268CF512A0E52E4F5BE5B94";

const char *g_mp71   = "1";
const char *g_mp25   = "7";
const char *l_mp1011 = "C589E3D7D64A6942A000";

/* mp9 in radices from 5 to 64 inclusive */
#define LOW_RADIX   5
#define HIGH_RADIX  64
const char *v_mp9[] = {
  "404041130042310320100141302000203430214122130002340212132414134210033",
  "44515230120451152500101352430105520150025145320010504454125502",
  "644641136612541136016610100564613624243140151310023515322",
  "173512120732412062323044435407317550316717172705712756",
  "265785018434285762514442046172754680368422060744852",
  "1411774500397290569709059837552310354075408897518",
  "184064268501499311A17746095910428222A241708032A",
  "47706011B225950B02BB45602AA039893118A85950892",
  "1A188C826B982353CB58422563AC602B783101671A86",
  "105957B358B89B018958908A9114BC3DDC410B77982",
  "CB7B3387E23452178846C55DD9D70C7CA9AEA78E8",
  "F74A2876A1432698923B0767DA19DCF3D71795EE",
  "17BF7C3673B76D7G7A5GA836277296F806E7453A",
  "2EBG8HH3HFA6185D6H0596AH96G24C966DD3HG2",
  "6G3HGBFEG8I3F25EAF61B904EIA40CFDH2124F",
  "10AHC3D29EBHDF3HD97905CG0JA8061855C3FI",
  "3BA5A55J5K699B2D09C38A4B237CH51IHA132",
  "EDEA90DJ0B5CB3FGG1C8587FEB99D3C143CA",
  "31M26JI1BBD56K3I028MML4EEDMAJK60LGLE",
  "GGG5M3142FKKG82EJ28111D70EMHC241E4E",
  "4446F4D5H10982023N297BF0DKBBHLLJB0I",
  "12E9DEEOBMKAKEP0IM284MIP7FO1O521M46",
  "85NN0HD48NN2FDDB1F5BMMKIB8CK20MDPK",
  "2D882A7A0O0JPCJ4APDRIB77IABAKDGJP2",
  "MFMCI0R7S27AAA3O3L2S8K44HKA7O02CN",
  "7IGQS73FFSHC50NNH44B6PTTNLC3M6H78",
  "2KLUB3U9850CSN6ANIDNIF1LB29MJ43LH",
  "UT52GTL18CJ9H4HR0TJTK6ESUFBHF5FE",
  "BTVL87QQBMUGF8PFWU4W3VU7U922QTMW",
  "4OG10HW0MSWJBIDEE2PDH24GA7RIHIAA",
  "1W8W9AX2DRUX48GXOLMK0PE42H0FEUWN",
  "SVWI84VBH069WR15W1U2VTK06USY8Z2",
  "CPTPNPDa5TYCPPNLALENT9IMX2GL0W2",
  "5QU21UJMRaUYYYYYN6GHSMPOYOXEEUY",
  "2O2Q7C6RPPB1SXJ9bR4035SPaQQ3H2W",
  "18d994IbT4PHbD7cGIPCRP00bbQO0bc",
  "NcDUEEWRO7XT76260WGeBHPVa72RdA",
  "BbX2WCF9VfSB5LPdJAdeXKV1fd6LC2",
  "60QDKW67P4JSQaTdQg7JE9ISafLaVU",
  "33ba9XbDbRdNF4BeDB2XYMhAVDaBdA",
  "1RIPZJA8gT5L5H7fTcaRhQ39geMMTc",
  "d65j70fBATjcDiidPYXUGcaBVVLME",
  "LKA9jhPabDG612TXWkhfT2gMXNIP2",
  "BgNaYhjfT0G8PBcYRP8khJCR3C9QE",
  "6Wk8RhJTAgDh10fYAiUVB1aM0HacG",
  "3dOCjaf78kd5EQNViUZWj3AfFL90I",
  "290VWkL3aiJoW4MBbHk0Z0bDo22Ni",
  "1DbDZ1hpPZNUDBUp6UigcJllEdC26",
  "dFSOLBUM7UZX8Vnc6qokGIOiFo1h",
  "NcoUYJOg0HVmKI9fR2ag0S8R2hrK",
  "EOpiJ5Te7oDe2pn8ZhAUKkhFHlZh",
  "8nXK8rp8neV8LWta1WDgd1QnlWsU",
  "5T3d6bcSBtHgrH9bCbu84tblaa7r",
  "3PlUDIYUvMqOVCir7AtquK5dWanq",
  "2A70gDPX2AtiicvIGGk9poiMtgvu",
  "1MjiRxjk10J6SVAxFguv9kZiUnIc",
  "rpre2vIDeb4h3sp50r1YBbtEx9L",
  "ZHcoip0AglDAfibrsUcJ9M1C8fm",
  "NHP18+eoe6uU54W49Kc6ZK7+bT2",
  "FTAA7QXGoQOaZi7PzePtFFN5vNk"
};

const unsigned char b_mp4[] = {
  0x01, 
#if MP_DIGIT_MAX > MP_32BIT_MAX
  0x00, 0x00, 0x00, 0x00,
#endif
  0x63, 0xDB, 0xC2, 0x26, 
  0x5B, 0x88, 0x26, 0x8D, 
  0xC8, 0x01, 0xC1, 0x0E, 
  0xA6, 0x84, 0x76, 0xB7, 
  0xBD, 0xE0, 0x09, 0x0F
};

/* Search for a test suite name in the names table  */
int  find_name(char *name);
void reason(char *fmt, ...);

/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

char g_intbuf[4096];  /* buffer for integer comparison   */
char a_intbuf[4096];  /* buffer for integer comparison   */
int  g_verbose = 1;   /* print out reasons for failure?  */
int  res;

#define IFOK(x) { if (MP_OKAY > (res = (x))) { \
  reason("test %s failed: error %d\n", #x, res); return 1; }}

int main(int argc, char *argv[])
{
  int which, res;

  srand((unsigned int)time(NULL));

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <test-suite> | list\n"
	    "Type '%s help' for assistance\n", argv[0], argv[0]);
    return 2;
  } else if(argc > 2) {
    if(strcmp(argv[2], "quiet") == 0)
      g_verbose = 0;
  }

  if(strcmp(argv[1], "help") == 0) {
    fprintf(stderr, "Help for mpi-test\n\n"
	    "This program is a test driver for the MPI library, which\n"
	    "tests all the various functions in the library to make sure\n"
	    "they are working correctly.  The syntax is:\n"
	    "    %s <suite-name>\n"
	    "...where <suite-name> is the name of the test you wish to\n"
	    "run.  To get a list of the tests, use '%s list'.\n\n"
	    "The program exits with a status of zero if the test passes,\n"
	    "or non-zero if it fails.  Ordinarily, failure is accompanied\n"
	    "by a diagnostic message to standard error.  To suppress this\n"
	    "add the keyword 'quiet' after the suite-name on the command\n"
	    "line.\n\n", argv[0], argv[0]);
    return 0;
  }

  if ((which = find_name(argv[1])) < 0) {
    fprintf(stderr, "%s: test suite '%s' is not known\n", argv[0], argv[1]);
    return 2;
  }

  if((res = (g_tests[which])()) < 0) {
    fprintf(stderr, "%s: test suite not implemented yet\n", argv[0]);
    return 2;
  } else {
    return res; 
  }

}

/*------------------------------------------------------------------------*/

int find_name(char *name)
{
  int ix = 0;
  
  while(ix < g_count) {
    if (strcmp(name, g_names[ix]) == 0)
      return ix;
    
    ++ix;
  }
  
  return -1;
}

/*------------------------------------------------------------------------*/

int test_list(void)
{
  int ix;
  
  fprintf(stderr, "There are currently %d test suites available\n",
	  g_count);
  
  for(ix = 1; ix < g_count; ix++)
    fprintf(stdout, "%-20s %s\n", g_names[ix], g_descs[ix]);
  
  return 0;
}

/*------------------------------------------------------------------------*/

int test_copy(void)
{
  mp_int  a, b;
  int     ix;

  mp_init(&a); mp_init(&b);

  mp_read_radix(&a, mp3, 16);
  mp_copy(&a, &b);

  if(SIGN(&a) != SIGN(&b) || USED(&a) != USED(&b)) {
    if(SIGN(&a) != SIGN(&b)) {
      reason("error: sign of original is %d, sign of copy is %d\n", 
	     SIGN(&a), SIGN(&b));
    } else {
      reason("error: original precision is %d, copy precision is %d\n",
	     USED(&a), USED(&b));
    }
    mp_clear(&a); mp_clear(&b);
    return 1;
  }

  for(ix = 0; ix < USED(&b); ix++) {
    if(DIGIT(&a, ix) != DIGIT(&b, ix)) {
      reason("error: digit %d " DIGIT_FMT " != " DIGIT_FMT "\n",
	     ix, DIGIT(&a, ix), DIGIT(&b, ix));
      mp_clear(&a); mp_clear(&b);
      return 1;
    }
  }
     
  mp_clear(&a); mp_clear(&b);
  return 0;
}

/*------------------------------------------------------------------------*/

int test_exch(void)
{
  mp_int  a, b;

  mp_init(&a); mp_init(&b);
  mp_read_radix(&a, mp7, 16); mp_read_radix(&b, mp1, 16);

  mp_exch(&a, &b);
  mp_toradix(&a, g_intbuf, 16);

  mp_clear(&a);
  if(strcmp(g_intbuf, mp1) != 0) {
    mp_clear(&b);
    reason("error: exchange failed\n");
    return 1;
  }

  mp_toradix(&b, g_intbuf, 16);

  mp_clear(&b);
  if(strcmp(g_intbuf, mp7) != 0) {
    reason("error: exchange failed\n");
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_zero(void)
{
  mp_int   a;

  mp_init(&a); mp_read_radix(&a, mp7, 16);
  mp_zero(&a);

  if(USED(&a) != 1 || DIGIT(&a, 1) != 0) {
    mp_toradix(&a, g_intbuf, 16);
    reason("error: result is %s\n", g_intbuf);
    mp_clear(&a);
    return 1;
  }

  mp_clear(&a);
  return 0;
}

/*------------------------------------------------------------------------*/

int test_set(void)
{
  mp_int   a;

  /* Test single digit set */
  mp_init(&a); mp_set(&a, 5);
  if(DIGIT(&a, 0) != 5) {
    mp_toradix(&a, g_intbuf, 16);
    reason("error: result is %s, expected 5\n", g_intbuf);
    mp_clear(&a);
    return 1;
  }

  /* Test integer set */
  mp_set_int(&a, -4938110);
  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a);
  if(strcmp(g_intbuf, mp5a) != 0) {
    reason("error: result is %s, expected %s\n", g_intbuf, mp5a);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_abs(void)
{
  mp_int  a;

  mp_init(&a); mp_read_radix(&a, mp4, 16);
  mp_abs(&a, &a);
  
  if(SIGN(&a) != ZPOS) {
    reason("error: sign of result is negative\n");
    mp_clear(&a);
    return 1;
  }

  mp_clear(&a);
  return 0;
}

/*------------------------------------------------------------------------*/

int test_neg(void)
{
  mp_int  a;
  mp_sign s;

  mp_init(&a); mp_read_radix(&a, mp4, 16);

  s = SIGN(&a);
  mp_neg(&a, &a);
  if(SIGN(&a) == s) {
    reason("error: sign of result is same as sign of nonzero input\n");
    mp_clear(&a);
    return 1;
  }

  mp_clear(&a);
  return 0;
}

/*------------------------------------------------------------------------*/

int test_add_d(void)
{
  mp_int  a;

  mp_init(&a);
  
  mp_read_radix(&a, mp5, 16);
  mp_add_d(&a, md4, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, s_mp5d4) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, s_mp5d4);
    mp_clear(&a);
    return 1;
  }

  mp_read_radix(&a, mp2, 16);
  mp_add_d(&a, md5, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, s_mp2d5) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, s_mp2d5);
    mp_clear(&a);
    return 1;
  }

  mp_clear(&a);
  return 0;
}

/*------------------------------------------------------------------------*/

int test_add(void)
{
  mp_int  a, b;
  int     res = 0;

  mp_init(&a); mp_init(&b);

  mp_read_radix(&a, mp1, 16); mp_read_radix(&b, mp3, 16);
  mp_add(&a, &b, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, s_mp13) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, s_mp13);
    res = 1; goto CLEANUP;
  }

  mp_read_radix(&a, mp4, 16);
  mp_add(&a, &b, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, s_mp34) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, s_mp34);
    res = 1; goto CLEANUP;
  }

  mp_read_radix(&a, mp4, 16); mp_read_radix(&b, mp6, 16);
  mp_add(&a, &b, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, s_mp46) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, s_mp46);
    res = 1; goto CLEANUP;
  }

  mp_read_radix(&a, mp14, 16); mp_read_radix(&b, mp15, 16);
  mp_add(&a, &b, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, s_mp1415) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, s_mp1415);
    res = 1;
  }

 CLEANUP:
  mp_clear(&a); mp_clear(&b);
  return res;
}

/*------------------------------------------------------------------------*/

int test_sub_d(void)
{
  mp_int   a;

  mp_init(&a);
  mp_read_radix(&a, mp5, 16);

  mp_sub_d(&a, md4, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, d_mp5d4) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, d_mp5d4);
    mp_clear(&a);
    return 1;
  }

  mp_read_radix(&a, mp6, 16);
  
  mp_sub_d(&a, md2, &a);
  mp_toradix(&a, g_intbuf, 16);
  
  mp_clear(&a);
  if(strcmp(g_intbuf, d_mp6d2) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, d_mp6d2);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_sub(void)
{
  mp_int  a, b;

  mp_init(&a); mp_init(&b);

  mp_read_radix(&a, mp1, 16); mp_read_radix(&b, mp2, 16);
  mp_sub(&a, &b, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, d_mp12) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, d_mp12);
    mp_clear(&a); mp_clear(&b);
    return 1;
  }

  mp_read_radix(&a, mp3, 16); mp_read_radix(&b, mp4, 16);
  mp_sub(&a, &b, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, d_mp34) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, d_mp34);
    mp_clear(&a); mp_clear(&b);
    return 1;
  }

  mp_clear(&a); mp_clear(&b);
  return 0;
}

/*------------------------------------------------------------------------*/

int test_mul_d(void)
{
  mp_int   a;

  mp_init(&a);
  mp_read_radix(&a, mp1, 16);

  IFOK( mp_mul_d(&a, md4, &a) );
  mp_toradix(&a, g_intbuf, 16);
  
  if(strcmp(g_intbuf, p_mp1d4) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, p_mp1d4);    
    mp_clear(&a);
    return 1;
  }

  mp_read_radix(&a, mp8, 16);
  IFOK( mp_mul_d(&a, md6, &a) );
  mp_toradix(&a, g_intbuf, 16);

  mp_clear(&a);
  if(strcmp(g_intbuf, p_mp8d6) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, p_mp8d6); 
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_mul(void)
{
  mp_int   a, b;
  int      res = 0;

  mp_init(&a); mp_init(&b);
  mp_read_radix(&a, mp1, 16); mp_read_radix(&b, mp2, 16);

  IFOK( mp_mul(&a, &b, &a) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, p_mp12) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, p_mp12);
    res = 1; goto CLEANUP;
  }

  mp_read_radix(&a, mp3, 16); mp_read_radix(&b, mp4, 16);
  IFOK( mp_mul(&a, &b, &a) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, p_mp34) !=0) {
    reason("error: computed %s, expected %s\n", g_intbuf, p_mp34);
    res = 1; goto CLEANUP;
  }

  mp_read_radix(&a, mp5, 16); mp_read_radix(&b, mp7, 16);
  IFOK( mp_mul(&a, &b, &a) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, p_mp57) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, p_mp57);
    res = 1; goto CLEANUP;
  }

  mp_read_radix(&a, mp11, 16); mp_read_radix(&b, mp13, 16);
  IFOK( mp_mul(&a, &b, &a) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, p_mp1113) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, p_mp1113);
    res = 1; goto CLEANUP;
  }

  mp_read_radix(&a, mp14, 16); mp_read_radix(&b, mp15, 16);
  IFOK( mp_mul(&a, &b, &a) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, p_mp1415) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, p_mp1415);
    res = 1;
  }
  mp_read_radix(&a, mp21, 10); mp_read_radix(&b, mp21, 10);

  IFOK( mp_mul(&a, &b, &a) );
  mp_toradix(&a, g_intbuf, 10);

  if(strcmp(g_intbuf, p_mp2121) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, p_mp2121);
    res = 1; goto CLEANUP;
  }

 CLEANUP:
  mp_clear(&a); mp_clear(&b);
  return res;

}

/*------------------------------------------------------------------------*/

int test_sqr(void)
{
  mp_int  a;

  mp_init(&a); mp_read_radix(&a, mp2, 16);

  mp_sqr(&a, &a);
  mp_toradix(&a, g_intbuf, 16);

  mp_clear(&a);
  if(strcmp(g_intbuf, p_mp22) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, p_mp22);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_div_d(void)
{
  mp_int    a, q;
  mp_digit  r;
  int       err = 0;

  mp_init(&a); mp_init(&q);
  mp_read_radix(&a, mp3, 16);

  IFOK( mp_div_d(&a, md6, &q, &r) );
  mp_toradix(&q, g_intbuf, 16);

  if(strcmp(g_intbuf, q_mp3d6) != 0) {
    reason("error: computed q = %s, expected %s\n", g_intbuf, q_mp3d6);
    ++err;
  }

  sprintf(g_intbuf, ZS_DIGIT_FMT, r);

  if(strcmp(g_intbuf, r_mp3d6) != 0) {
    reason("error: computed r = %s, expected %s\n", g_intbuf, r_mp3d6);
    ++err;
  }

  mp_read_radix(&a, mp9, 16);
  IFOK( mp_div_d(&a, 16, &q, &r) );
  mp_toradix(&q, g_intbuf, 16);

  if(strcmp(g_intbuf, q_mp9c16) != 0) {
    reason("error: computed q = %s, expected %s\n", g_intbuf, q_mp9c16);
    ++err;
  }

  sprintf(g_intbuf, ZS_DIGIT_FMT, r);

  if(strcmp(g_intbuf, r_mp9c16) != 0) {
    reason("error: computed r = %s, expected %s\n", g_intbuf, r_mp9c16);
    ++err;
  }

  mp_clear(&a); mp_clear(&q);
  return err;
}

/*------------------------------------------------------------------------*/

int test_div_2(void)
{
  mp_int  a;

  mp_init(&a); mp_read_radix(&a, mp7, 16);
  IFOK( mp_div_2(&a, &a) );
  mp_toradix(&a, g_intbuf, 16);

  mp_clear(&a);
  if(strcmp(g_intbuf, q_mp7c2) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, q_mp7c2);
    return 1;
  }
    
  return 0;
}

/*------------------------------------------------------------------------*/

int test_div_2d(void)
{
  mp_int  a, q, r;

  mp_init(&q); mp_init(&r);
  mp_init(&a); mp_read_radix(&a, mp13, 16);

  IFOK( mp_div_2d(&a, 64, &q, &r) );
  mp_clear(&a);

  mp_toradix(&q, g_intbuf, 16);

  if(strcmp(g_intbuf, q_mp13c) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, q_mp13c);
    mp_clear(&q); mp_clear(&r);
    return 1;
  }

  mp_clear(&q);

  mp_toradix(&r, g_intbuf, 16);
  if(strcmp(g_intbuf, r_mp13c) != 0) {
    reason("error, computed %s, expected %s\n", g_intbuf, r_mp13c);
    mp_clear(&r);
    return 1;
  }

  mp_clear(&r);
  
  return 0;
}

/*------------------------------------------------------------------------*/

int test_div(void)
{
  mp_int  a, b, r;
  int     err = 0;

  mp_init(&a); mp_init(&b); mp_init(&r);

  mp_read_radix(&a, mp4, 16); mp_read_radix(&b, mp2, 16);
  IFOK( mp_div(&a, &b, &a, &r) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, q_mp42) != 0) {
    reason("error: test 1 computed quot %s, expected %s\n", g_intbuf, q_mp42);
    ++err;
  }

  mp_toradix(&r, g_intbuf, 16);

  if(strcmp(g_intbuf, r_mp42) != 0) {
    reason("error: test 1 computed rem %s, expected %s\n", g_intbuf, r_mp42);
    ++err;
  }

  mp_read_radix(&a, mp4, 16); mp_read_radix(&b, mp5a, 16);
  IFOK( mp_div(&a, &b, &a, &r) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, q_mp45a) != 0) {
    reason("error: test 2 computed quot %s, expected %s\n", g_intbuf, q_mp45a);
    ++err;
  }

  mp_toradix(&r, g_intbuf, 16);

  if(strcmp(g_intbuf, r_mp45a) != 0) {
    reason("error: test 2 computed rem %s, expected %s\n", g_intbuf, r_mp45a);
    ++err;
  }

  mp_read_radix(&a, mp14, 16); mp_read_radix(&b, mp4, 16);
  IFOK( mp_div(&a, &b, &a, &r) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, q_mp1404) != 0) {
    reason("error: test 3 computed quot %s, expected %s\n", g_intbuf, q_mp1404);
    ++err;
  }

  mp_toradix(&r, g_intbuf, 16);
  
  if(strcmp(g_intbuf, r_mp1404) != 0) {
    reason("error: test 3 computed rem %s, expected %s\n", g_intbuf, r_mp1404);
    ++err;
  }

  mp_clear(&a); mp_clear(&b); mp_clear(&r);

  return err;
}

/*------------------------------------------------------------------------*/

int test_expt_d(void)
{
  mp_int   a;

  mp_init(&a); mp_read_radix(&a, mp5, 16);
  mp_expt_d(&a, md9, &a);
  mp_toradix(&a, g_intbuf, 16);

  mp_clear(&a);
  if(strcmp(g_intbuf, e_mp5d9) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, e_mp5d9);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_expt(void)
{
  mp_int   a, b;

  mp_init(&a); mp_init(&b);
  mp_read_radix(&a, mp7, 16); mp_read_radix(&b, mp8, 16);

  mp_expt(&a, &b, &a);
  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a); mp_clear(&b);

  if(strcmp(g_intbuf, e_mp78) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, e_mp78);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_2expt(void)
{
  mp_int   a;

  mp_init(&a);
  mp_2expt(&a, md3);
  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a);

  if(strcmp(g_intbuf, e_mpc2d3) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, e_mpc2d3);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_sqrt(void)
{
  mp_int  a;
  int     res = 0;

  mp_init(&a); mp_read_radix(&a, mp9, 16);
  mp_sqrt(&a, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, t_mp9) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, t_mp9);
    res = 1; goto CLEANUP;
  }

  mp_read_radix(&a, mp15, 16);
  mp_sqrt(&a, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, t_mp15) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, t_mp15);
    res = 1;
  }

 CLEANUP:
  mp_clear(&a);
  return res;
}

/*------------------------------------------------------------------------*/

int test_mod_d(void)
{
  mp_int     a;
  mp_digit   r;

  mp_init(&a); mp_read_radix(&a, mp5, 16);
  IFOK( mp_mod_d(&a, md5, &r) );
  sprintf(g_intbuf, ZS_DIGIT_FMT, r);
  mp_clear(&a);

  if(strcmp(g_intbuf, r_mp5d5) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, r_mp5d5);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_mod(void)
{
  mp_int  a, m;

  mp_init(&a); mp_init(&m);
  mp_read_radix(&a, mp4, 16); mp_read_radix(&m, mp7, 16);
  IFOK( mp_mod(&a, &m, &a) );
  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a); mp_clear(&m);

  if(strcmp(g_intbuf, r_mp47) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, r_mp47);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_addmod(void)
{
  mp_int a, b, m;

  mp_init(&a); mp_init(&b); mp_init(&m);
  mp_read_radix(&a, mp3, 16); mp_read_radix(&b, mp4, 16);
  mp_read_radix(&m, mp5, 16);

  IFOK( mp_addmod(&a, &b, &m, &a) );
  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a); mp_clear(&b); mp_clear(&m);

  if(strcmp(g_intbuf, ms_mp345) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, ms_mp345);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_submod(void)
{
  mp_int a, b, m;

  mp_init(&a); mp_init(&b); mp_init(&m);
  mp_read_radix(&a, mp3, 16); mp_read_radix(&b, mp4, 16);
  mp_read_radix(&m, mp5, 16);

  IFOK( mp_submod(&a, &b, &m, &a) );
  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a); mp_clear(&b); mp_clear(&m);

  if(strcmp(g_intbuf, md_mp345) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, md_mp345);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_mulmod(void)
{
  mp_int a, b, m;

  mp_init(&a); mp_init(&b); mp_init(&m);
  mp_read_radix(&a, mp3, 16); mp_read_radix(&b, mp4, 16);
  mp_read_radix(&m, mp5, 16);

  IFOK( mp_mulmod(&a, &b, &m, &a) );
  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a); mp_clear(&b); mp_clear(&m);

  if(strcmp(g_intbuf, mp_mp345) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, mp_mp345);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_sqrmod(void)
{
  mp_int a, m;

  mp_init(&a); mp_init(&m);
  mp_read_radix(&a, mp3, 16); mp_read_radix(&m, mp5, 16);

  IFOK( mp_sqrmod(&a, &m, &a) );
  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a); mp_clear(&m);

  if(strcmp(g_intbuf, mp_mp335) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, mp_mp335);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_exptmod(void)
{
  mp_int  a, b, m;
  int     res = 0;

  mp_init(&a); mp_init(&b); mp_init(&m);
  mp_read_radix(&a, mp8, 16); mp_read_radix(&b, mp1, 16);
  mp_read_radix(&m, mp7, 16);

  IFOK( mp_exptmod(&a, &b, &m, &a) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, me_mp817) != 0) {
    reason("case 1: error: computed %s, expected %s\n", g_intbuf, me_mp817);
    res = 1; goto CLEANUP;
  }

  mp_read_radix(&a, mp1, 16); mp_read_radix(&b, mp5, 16);
  mp_read_radix(&m, mp12, 16);

  IFOK( mp_exptmod(&a, &b, &m, &a) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, me_mp1512) != 0) {
    reason("case 2: error: computed %s, expected %s\n", g_intbuf, me_mp1512);
    res = 1; goto CLEANUP;
  }

  mp_read_radix(&a, mp5, 16); mp_read_radix(&b, mp1, 16);
  mp_read_radix(&m, mp14, 16);

  IFOK( mp_exptmod(&a, &b, &m, &a) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, me_mp5114) != 0) {
    reason("case 3: error: computed %s, expected %s\n", g_intbuf, me_mp5114);
    res = 1;
  }

  mp_read_radix(&a, mp16, 16); mp_read_radix(&b, mp17, 16);
  mp_read_radix(&m, mp18, 16);

  IFOK( mp_exptmod(&a, &b, &m, &a) );
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, me_mp161718) != 0) {
    reason("case 4: error: computed %s, expected %s\n", g_intbuf, me_mp161718);
    res = 1;
  }

 CLEANUP:
  mp_clear(&a); mp_clear(&b); mp_clear(&m);
  return res;
}

/*------------------------------------------------------------------------*/

int test_exptmod_d(void)
{
  mp_int  a, m;

  mp_init(&a); mp_init(&m);
  mp_read_radix(&a, mp5, 16); mp_read_radix(&m, mp7, 16);

  IFOK( mp_exptmod_d(&a, md4, &m, &a) );
  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a); mp_clear(&m);

  if(strcmp(g_intbuf, me_mp5d47) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, me_mp5d47);
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------*/

int test_invmod(void)
{
  mp_int  a, m, c;
  mp_int  p1, p2, p3, p4, p5;
  mp_int  t1, t2, t3, t4;
  mp_err  res;

  /* 5 128-bit primes. */
  static const char ivp1[] = { "AAD8A5A2A2BEF644BAEE7DB0CA643719" };
  static const char ivp2[] = { "CB371AD2B79A90BCC88D0430663E40B9" };
  static const char ivp3[] = { "C6C818D4DF2618406CA09280C0400099" };
  static const char ivp4[] = { "CE949C04512E68918006B1F0D7E93F27" };
  static const char ivp5[] = { "F8EE999B6416645040687440E0B89F51" };

  mp_init(&a); mp_init(&m);
  mp_read_radix(&a, mp2, 16); mp_read_radix(&m, mp7, 16);

  IFOK( mp_invmod(&a, &m, &a) );

  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a); mp_clear(&m);

  if(strcmp(g_intbuf, i_mp27) != 0) {
    reason("error: invmod test 1 computed %s, expected %s\n", g_intbuf, i_mp27);
    return 1;
  }

  mp_init(&a); mp_init(&m);
  mp_read_radix(&a, mp20, 16); mp_read_radix(&m, mp19, 16);

  IFOK( mp_invmod(&a, &m, &a) );

  mp_toradix(&a, g_intbuf, 16);
  mp_clear(&a); mp_clear(&m);

  if(strcmp(g_intbuf, i_mp2019) != 0) {
    reason("error: invmod test 2 computed %s, expected %s\n", g_intbuf, i_mp2019);
    return 1;
  }

/* Need the following test cases:
  Odd modulus
    - a is odd,      relatively prime to m
    - a is odd,  not relatively prime to m
    - a is even,     relatively prime to m
    - a is even, not relatively prime to m
  Even modulus
    - a is even  (should fail)
    - a is odd,  not relatively prime to m
    - a is odd,      relatively prime to m,
      m is not a power of 2
	- m has factor 2**k, k < 32
	- m has factor 2**k, k > 32
      m is a power of 2, 2**k
	- k < 32
	- k > 32
*/

  mp_init(&a);  mp_init(&m);  mp_init(&c);  
  mp_init(&p1); mp_init(&p2); mp_init(&p3); mp_init(&p4); mp_init(&p5); 
  mp_init(&t1); mp_init(&t2); mp_init(&t3); mp_init(&t4); 

  mp_read_radix(&p1, ivp1, 16);
  mp_read_radix(&p2, ivp2, 16);
  mp_read_radix(&p3, ivp3, 16);
  mp_read_radix(&p4, ivp4, 16);
  mp_read_radix(&p5, ivp5, 16);

  IFOK( mp_2expt(&t2, 68) );	/* t2 = 2**68 */
  IFOK( mp_2expt(&t3, 128) );	/* t3 = 2**128 */
  IFOK( mp_2expt(&t4, 31) );	/* t4 = 2**31 */

/* test 3: Odd modulus - a is odd, relatively prime to m */

  IFOK( mp_mul(&p1, &p2, &a) );
  IFOK( mp_mul(&p3, &p4, &m) );
  IFOK( mp_invmod(&a, &m, &t1) );
  IFOK( mp_invmod_xgcd(&a, &m, &c) );

  if (mp_cmp(&t1, &c) != 0) {
    mp_toradix(&t1, g_intbuf, 16);
    mp_toradix(&c,  a_intbuf, 16);
    reason("error: invmod test 3 computed %s, expected %s\n", 
           g_intbuf, a_intbuf);
    return 1;
  }
  mp_clear(&a);  mp_clear(&t1); mp_clear(&c);  
  mp_init(&a);   mp_init(&t1);  mp_init(&c);   

/* test 4: Odd modulus - a is odd, NOT relatively prime to m */

  IFOK( mp_mul(&p1, &p3, &a) );
  /* reuse same m as before */

  res = mp_invmod_xgcd(&a, &m, &c);
  if (res != MP_UNDEF) 
    goto CLEANUP4;

  res = mp_invmod(&a, &m, &t1); /* we expect this to fail. */
  if (res != MP_UNDEF) {
CLEANUP4:
    reason("error: invmod test 4 succeeded, should have failed.\n");
    return 1;
  }
  mp_clear(&a);  mp_clear(&t1); mp_clear(&c);  
  mp_init(&a);   mp_init(&t1);  mp_init(&c);   

/* test 5: Odd modulus - a is even, relatively prime to m */

  IFOK( mp_mul(&p1, &t2, &a) );
  /* reuse m */
  IFOK( mp_invmod(&a, &m, &t1) );
  IFOK( mp_invmod_xgcd(&a, &m, &c) );

  if (mp_cmp(&t1, &c) != 0) {
    mp_toradix(&t1, g_intbuf, 16);
    mp_toradix(&c,  a_intbuf, 16);
    reason("error: invmod test 5 computed %s, expected %s\n", 
           g_intbuf, a_intbuf);
    return 1;
  }
  mp_clear(&a);  mp_clear(&t1); mp_clear(&c);  
  mp_init(&a);   mp_init(&t1);  mp_init(&c);   

/* test 6: Odd modulus - a is odd, NOT relatively prime to m */

  /* reuse t2 */
  IFOK( mp_mul(&t2, &p3, &a) );
  /* reuse same m as before */

  res = mp_invmod_xgcd(&a, &m, &c);
  if (res != MP_UNDEF) 
    goto CLEANUP6;

  res = mp_invmod(&a, &m, &t1); /* we expect this to fail. */
  if (res != MP_UNDEF) {
CLEANUP6:
    reason("error: invmod test 6 succeeded, should have failed.\n");
    return 1;
  }
  mp_clear(&a);  mp_clear(&m); mp_clear(&c);  mp_clear(&t1); 
  mp_init(&a);   mp_init(&m);  mp_init(&c);   mp_init(&t1); 

/* test 7: Even modulus, even a, should fail */

  IFOK( mp_mul(&p3, &t3, &m) ); /* even m */
  /* reuse t2 */
  IFOK( mp_mul(&p1, &t2, &a) ); /* even a */

  res = mp_invmod_xgcd(&a, &m, &c);
  if (res != MP_UNDEF) 
    goto CLEANUP7;

  res = mp_invmod(&a, &m, &t1); /* we expect this to fail. */
  if (res != MP_UNDEF) {
CLEANUP7:
    reason("error: invmod test 7 succeeded, should have failed.\n");
    return 1;
  }
  mp_clear(&a);  mp_clear(&c);  mp_clear(&t1); 
  mp_init(&a);   mp_init(&c);   mp_init(&t1); 

/* test 8: Even modulus    - a is odd,  not relatively prime to m */

  /* reuse m */
  IFOK( mp_mul(&p3, &p1, &a) ); /* even a */

  res = mp_invmod_xgcd(&a, &m, &c);
  if (res != MP_UNDEF) 
    goto CLEANUP8;

  res = mp_invmod(&a, &m, &t1); /* we expect this to fail. */
  if (res != MP_UNDEF) {
CLEANUP8:
    reason("error: invmod test 8 succeeded, should have failed.\n");
    return 1;
  }
  mp_clear(&a);  mp_clear(&m); mp_clear(&c);  mp_clear(&t1); 
  mp_init(&a);   mp_init(&m);  mp_init(&c);   mp_init(&t1); 

/* test 9: Even modulus    - m has factor 2**k, k < 32
 *	                   - a is odd, relatively prime to m,
 */
  IFOK( mp_mul(&p3, &t4, &m) ); /* even m */
  IFOK( mp_mul(&p1, &p2, &a) );
  IFOK( mp_invmod(&a, &m, &t1) );
  IFOK( mp_invmod_xgcd(&a, &m, &c) );

  if (mp_cmp(&t1, &c) != 0) {
    mp_toradix(&t1, g_intbuf, 16);
    mp_toradix(&c,  a_intbuf, 16);
    reason("error: invmod test 9 computed %s, expected %s\n", 
           g_intbuf, a_intbuf);
    return 1;
  }
  mp_clear(&m);  mp_clear(&t1); mp_clear(&c);  
  mp_init(&m);   mp_init(&t1);  mp_init(&c);   

/* test 10: Even modulus    - m has factor 2**k, k > 32
 *	                    - a is odd, relatively prime to m,
 */
  IFOK( mp_mul(&p3, &t3, &m) ); /* even m */
  /* reuse a */
  IFOK( mp_invmod(&a, &m, &t1) );
  IFOK( mp_invmod_xgcd(&a, &m, &c) );

  if (mp_cmp(&t1, &c) != 0) {
    mp_toradix(&t1, g_intbuf, 16);
    mp_toradix(&c,  a_intbuf, 16);
    reason("error: invmod test 10 computed %s, expected %s\n", 
           g_intbuf, a_intbuf);
    return 1;
  }
  mp_clear(&t1); mp_clear(&c);  
  mp_init(&t1);  mp_init(&c);   

/* test 11: Even modulus    - m is a power of 2, 2**k | k < 32
 *                          - a is odd, relatively prime to m,
 */
  IFOK( mp_invmod(&a, &t4, &t1) );
  IFOK( mp_invmod_xgcd(&a, &t4, &c) );

  if (mp_cmp(&t1, &c) != 0) {
    mp_toradix(&t1, g_intbuf, 16);
    mp_toradix(&c,  a_intbuf, 16);
    reason("error: invmod test 11 computed %s, expected %s\n", 
           g_intbuf, a_intbuf);
    return 1;
  }
  mp_clear(&t1); mp_clear(&c);  
  mp_init(&t1);  mp_init(&c);   

/* test 12: Even modulus    - m is a power of 2, 2**k | k > 32
 *                          - a is odd, relatively prime to m,
 */
  IFOK( mp_invmod(&a, &t3, &t1) );
  IFOK( mp_invmod_xgcd(&a, &t3, &c) );

  if (mp_cmp(&t1, &c) != 0) {
    mp_toradix(&t1, g_intbuf, 16);
    mp_toradix(&c,  a_intbuf, 16);
    reason("error: invmod test 12 computed %s, expected %s\n", 
           g_intbuf, a_intbuf);
    return 1;
  }

  mp_clear(&a);  mp_clear(&m);  mp_clear(&c);  
  mp_clear(&t1); mp_clear(&t2); mp_clear(&t3); mp_clear(&t4); 
  mp_clear(&p1); mp_clear(&p2); mp_clear(&p3); mp_clear(&p4); mp_clear(&p5); 

  return 0;
}

/*------------------------------------------------------------------------*/

int test_cmp_d(void)
{
  mp_int  a;

  mp_init(&a); mp_read_radix(&a, mp8, 16);

  if(mp_cmp_d(&a, md8) >= 0) {
    reason("error: %s >= " DIGIT_FMT "\n", mp8, md8);
    mp_clear(&a);
    return 1;
  }

  mp_read_radix(&a, mp5, 16);

  if(mp_cmp_d(&a, md8) <= 0) {
    reason("error: %s <= " DIGIT_FMT "\n", mp5, md8);
    mp_clear(&a);
    return 1;
  }

  mp_read_radix(&a, mp6, 16);

  if(mp_cmp_d(&a, md1) != 0) {
    reason("error: %s != " DIGIT_FMT "\n", mp6, md1);
    mp_clear(&a);
    return 1;
  }

  mp_clear(&a);
  return 0;

}

/*------------------------------------------------------------------------*/

int test_cmp_z(void)
{
  mp_int  a;

  mp_init(&a); mp_read_radix(&a, mp6, 16);

  if(mp_cmp_z(&a) != 0) {
    reason("error: someone thinks a zero value is non-zero\n");
    mp_clear(&a);
    return 1;
  }

  mp_read_radix(&a, mp1, 16);
  
  if(mp_cmp_z(&a) <= 0) {
    reason("error: someone thinks a positive value is non-positive\n");
    mp_clear(&a);
    return 1;
  }

  mp_read_radix(&a, mp4, 16);

  if(mp_cmp_z(&a) >= 0) {
    reason("error: someone thinks a negative value is non-negative\n");
    mp_clear(&a);
    return 1;
  }

  mp_clear(&a);
  return 0;
}

/*------------------------------------------------------------------------*/

int test_cmp(void)
{
  mp_int  a, b;

  mp_init(&a); mp_init(&b);
  mp_read_radix(&a, mp3, 16); mp_read_radix(&b, mp4, 16);

  if(mp_cmp(&a, &b) <= 0) {
    reason("error: %s <= %s\n", mp3, mp4);
    mp_clear(&a); mp_clear(&b);
    return 1;
  }

  mp_read_radix(&b, mp3, 16);
  if(mp_cmp(&a, &b) != 0) {
    reason("error: %s != %s\n", mp3, mp3);
    mp_clear(&a); mp_clear(&b);
    return 1;
  }

  mp_read_radix(&a, mp5, 16);
  if(mp_cmp(&a, &b) >= 0) {
    reason("error: %s >= %s\n", mp5, mp3);
    mp_clear(&a); mp_clear(&b);
    return 1;
  }

  mp_read_radix(&a, mp5a, 16);
  if(mp_cmp_int(&a, 1000000) >= 0 ||
     (mp_cmp_int(&a, -5000000) <= 0) ||
     (mp_cmp_int(&a, -4938110) != 0)) {
    reason("error: long integer comparison failed (%s)", mp5a);
    mp_clear(&a); mp_clear(&b);
    return 1;
  }

  mp_clear(&a); mp_clear(&b);
  return 0;
}

/*------------------------------------------------------------------------*/

int test_cmp_mag(void)
{
  mp_int  a, b;

  mp_init(&a); mp_init(&b);
  mp_read_radix(&a, mp5, 16); mp_read_radix(&b, mp4, 16);

  if(mp_cmp_mag(&a, &b) >= 0) {
    reason("error: %s >= %s\n", mp5, mp4);
    mp_clear(&a); mp_clear(&b);
    return 1;
  }

  mp_read_radix(&b, mp5, 16);
  if(mp_cmp_mag(&a, &b) != 0) {
    reason("error: %s != %s\n", mp5, mp5);
    mp_clear(&a); mp_clear(&b);
    return 1;
  }

  mp_read_radix(&a, mp1, 16);
  if(mp_cmp_mag(&b, &a) >= 0) {
    reason("error: %s >= %s\n", mp5, mp1);
    mp_clear(&a); mp_clear(&b);
    return 1;
  }

  mp_clear(&a); mp_clear(&b);
  return 0;

}

/*------------------------------------------------------------------------*/

int test_parity(void)
{
  mp_int  a;

  mp_init(&a); mp_read_radix(&a, mp1, 16);

  if(!mp_isodd(&a)) {
    reason("error: expected operand to be odd, but it isn't\n");
    mp_clear(&a);
    return 1;
  }

  mp_read_radix(&a, mp6, 16);
  
  if(!mp_iseven(&a)) {
    reason("error: expected operand to be even, but it isn't\n");
    mp_clear(&a);
    return 1;
  }

  mp_clear(&a);
  return 0;
}

/*------------------------------------------------------------------------*/

int test_gcd(void)
{
  mp_int  a, b;
  int     out = 0;

  mp_init(&a); mp_init(&b);
  mp_read_radix(&a, mp7, 16); mp_read_radix(&b, mp1, 16);

  mp_gcd(&a, &b, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, g_mp71) != 0) {
    reason("error: computed %s, expected %s\n", g_intbuf, g_mp71);
    out = 1;
  }

  mp_clear(&a); mp_clear(&b);
  return out;

}

/*------------------------------------------------------------------------*/

int test_lcm(void)
{
  mp_int  a, b;
  int     out = 0;

  mp_init(&a); mp_init(&b);
  mp_read_radix(&a, mp10, 16); mp_read_radix(&b, mp11, 16);

  mp_lcm(&a, &b, &a);
  mp_toradix(&a, g_intbuf, 16);

  if(strcmp(g_intbuf, l_mp1011) != 0) {
    reason("error: computed %s, expected%s\n", g_intbuf, l_mp1011);
    out = 1;
  }

  mp_clear(&a); mp_clear(&b);

  return out;

}

/*------------------------------------------------------------------------*/

int test_convert(void)
{
  int    ix;
  mp_int a;

  mp_init(&a); mp_read_radix(&a, mp9, 16);

  for(ix = LOW_RADIX; ix <= HIGH_RADIX; ix++) {
    mp_toradix(&a, g_intbuf, ix);

    if(strcmp(g_intbuf, v_mp9[ix - LOW_RADIX]) != 0) {
      reason("error: radix %d, computed %s, expected %s\n",
	     ix, g_intbuf, v_mp9[ix - LOW_RADIX]);
      mp_clear(&a);
      return 1;
    }
  }

  mp_clear(&a);
  return 0;
}

/*------------------------------------------------------------------------*/

int test_raw(void)
{
  int    len, out = 0;
  mp_int a;
  char   *buf;

  mp_init(&a); mp_read_radix(&a, mp4, 16);

  len = mp_raw_size(&a);
  if(len != sizeof(b_mp4)) {
    reason("error: test_raw: expected length %d, computed %d\n", sizeof(b_mp4),
	   len);
    mp_clear(&a);
    return 1;
  }

  buf = calloc(len, sizeof(char));
  mp_toraw(&a, buf);

  if(memcmp(buf, b_mp4, sizeof(b_mp4)) != 0) {
    reason("error: test_raw: binary output does not match test vector\n");
    out = 1;
  }

  free(buf);
  mp_clear(&a);

  return out;

}

/*------------------------------------------------------------------------*/

int test_pprime(void)
{
  mp_int   p;
  int      err = 0;
  mp_err   res;

  mp_init(&p);
  mp_read_radix(&p, mp7, 16);

  if(mpp_pprime(&p, 5) != MP_YES) {
    reason("error: %s failed Rabin-Miller test, but is prime\n", mp7);
    err = 1;
  }

  IFOK( mp_set_int(&p, 9) );
  res = mpp_pprime(&p, 50);
  if (res == MP_YES) {
    reason("error: 9 is composite but passed Rabin-Miller test\n");
    err = 1;
  } else if (res != MP_NO) {
    reason("test mpp_pprime(9, 50) failed: error %d\n", res); 
    err = 1;
  }

  IFOK( mp_set_int(&p, 15) );
  res = mpp_pprime(&p, 50);
  if (res == MP_YES) {
    reason("error: 15 is composite but passed Rabin-Miller test\n");
    err = 1;
  } else if (res != MP_NO) {
    reason("test mpp_pprime(15, 50) failed: error %d\n", res); 
    err = 1;
  }

  mp_clear(&p);

  return err;

}

/*------------------------------------------------------------------------*/

int test_fermat(void)
{
  mp_int p;
  mp_err res;
  int    err = 0;

  mp_init(&p);
  mp_read_radix(&p, mp7, 16);
  
  if((res = mpp_fermat(&p, 2)) != MP_YES) {
    reason("error: %s failed Fermat test on 2: %s\n", mp7, 
	   mp_strerror(res));
    ++err;
  }

  if((res = mpp_fermat(&p, 3)) != MP_YES) {
    reason("error: %s failed Fermat test on 3: %s\n", mp7, 
	   mp_strerror(res));
    ++err;
  }

  mp_clear(&p);

  return err;

}

/*------------------------------------------------------------------------*/
/* Like fprintf(), but only if we are behaving in a verbose manner        */

void reason(char *fmt, ...)
{
  va_list    ap;

  if(!g_verbose)
    return;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

/*------------------------------------------------------------------------*/
/* HERE THERE BE DRAGONS                                                  */
