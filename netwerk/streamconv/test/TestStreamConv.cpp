/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamConverter.h"
#include "nsIRegistry.h"
#include "nsIFactory.h"
#include "nsIStringStream.h"
#include "nsIIOService.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#include "nspr.h"


#define ASYNC_TEST // undefine this if you want to test sycnronous conversion.

/////////////////////////////////
// Event pump setup
/////////////////////////////////
#include "nsIEventQueueService.h"
#ifdef WIN32 
#include <windows.h>
#endif
#ifdef XP_OS2
#include <os2.h>
#endif

static int gKeepRunning = 0;
static nsIEventQueue* gEventQ = nsnull;
/////////////////////////////////
// Event pump END
/////////////////////////////////


/////////////////////////////////
// Test converters include
/////////////////////////////////
#include "Converters.h"

// CID setup
static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kComponentManagerCID,       NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kRegistryCID,               NS_REGISTRY_CID);

////////////////////////////////////////////////////////////////////////
// EndListener - This listener is the final one in the chain. It
//   receives the fully converted data, although it doesn't do anything with
//   the data.
////////////////////////////////////////////////////////////////////////
class EndListener : public nsIStreamListener {
public:
    // nsISupports declaration
    NS_DECL_ISUPPORTS

    EndListener() {NS_INIT_ISUPPORTS();};

    // nsIStreamListener method
    NS_IMETHOD OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, nsIInputStream *inStr, 
                               PRUint32 sourceOffset, PRUint32 count)
    {
        nsresult rv;
        PRUint32 read, len;
        rv = inStr->Available(&len);
        if (NS_FAILED(rv)) return rv;

        char *buffer = (char*)nsMemory::Alloc(len + 1);
        if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

        rv = inStr->Read(buffer, len, &read);
        buffer[len] = '\0';
        if (NS_SUCCEEDED(rv)) {
            printf("CONTEXT %p: Received %u bytes and the following data: \n %s\n\n", ctxt, read, buffer);
        }
        nsMemory::Free(buffer);

        return NS_OK;
    }

    // nsIStreamObserver methods
    NS_IMETHOD OnStartRequest(nsIChannel *channel, nsISupports *ctxt) 
    {
        return NS_OK;
    }

    NS_IMETHOD OnStopRequest(nsIChannel *channel, nsISupports *ctxt, 
                             nsresult aStatus, const PRUnichar* aStatusArg)
    {
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(EndListener, NS_GET_IID(nsIStreamListener));
////////////////////////////////////////////////////////////////////////
// EndListener END
////////////////////////////////////////////////////////////////////////


nsresult SendData(const char * aData, nsIStreamListener* aListener, nsIChannel* aChannel) {
    nsString data;
    data.AssignWithConversion(aData);
    nsCOMPtr<nsIInputStream> dataStream;
    nsCOMPtr<nsISupports> sup;
    nsresult rv = NS_NewStringInputStream(getter_AddRefs(sup), data);
    if (NS_FAILED(rv)) return rv;
    dataStream = do_QueryInterface(sup, &rv);
    if (NS_FAILED(rv)) return rv;
    return aListener->OnDataAvailable(aChannel, nsnull, dataStream, 0, -1);
}
#define SEND_DATA(x) SendData(x, converterListener, dummyChannel)

int
main(int argc, char* argv[])
{
    nsresult rv;

    // Create the Event Queue for this thread...
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);

    rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
    if (NS_FAILED(rv)) return rv;

    ///////////////////////////////////////////
    // BEGIN - Stream converter registration
    //   All stream converters must register with the ComponentManager, _and_ make
    //   a registry entry.
    ///////////////////////////////////////////
    TestConverterFactory *convFactory = new TestConverterFactory(kTestConverterCID, "TestConverter", NS_ISTREAMCONVERTER_KEY);
    nsIFactory *convFactSup = nsnull;
    rv = convFactory->QueryInterface(NS_GET_IID(nsIFactory), (void**)&convFactSup);
    if (NS_FAILED(rv)) return rv;

    TestConverterFactory *convFactory1 = new TestConverterFactory(kTestConverter1CID, "TestConverter1", NS_ISTREAMCONVERTER_KEY);
    nsIFactory *convFactSup1 = nsnull;
    rv = convFactory1->QueryInterface(NS_GET_IID(nsIFactory), (void**)&convFactSup1);
    if (NS_FAILED(rv)) return rv;

    // register the TestConverter with the component manager. One contractid registration
    // per conversion pair (from - to pair).
    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=a/foo&to=b/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverter1CID,
                                             "TestConverter1",
                                             NS_ISTREAMCONVERTER_KEY "?from=b/foo&to=c/foo",
                                             convFactSup1,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=b/foo&to=d/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=c/foo&to=d/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=d/foo&to=e/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=d/foo&to=f/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::RegisterFactory(kTestConverterCID,
                                             "TestConverter",
                                             NS_ISTREAMCONVERTER_KEY "?from=t/foo&to=k/foo",
                                             convFactSup,
                                             PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    
    // register the converters with the registry. One contractid registration
    // per conversion pair.
    NS_WITH_SERVICE(nsIRegistry, registry, kRegistryCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // open the registry
    rv = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    if (NS_FAILED(rv)) return rv;

    // set the key
    nsRegistryKey key, key1;

    rv = registry->AddSubtree(nsIRegistry::Common, NS_ISTREAMCONVERTER_KEY, &key);
    if (NS_FAILED(rv)) return rv;
    rv = registry->AddSubtreeRaw(key, "?from=a/foo&to=b/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=b/foo&to=c/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=multipart/x-mixed-replace&to=text/html", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=b/foo&to=d/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=c/foo&to=d/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=d/foo&to=e/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=d/foo&to=f/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=t/foo&to=k/foo", &key1);
    if (NS_FAILED(rv)) return rv;

    registry = 0; // close the registry

    NS_WITH_SERVICE(nsIStreamConverterService, StreamConvService, kStreamConverterServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsString fromStr;
    fromStr.AssignWithConversion("multipart/x-mixed-replace");
    nsString toStr;
    toStr.AssignWithConversion("text/html");
    

#ifdef ASYNC_TEST
    // ASYNCRONOUS conversion

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // we need a dummy channel for the async calls.
    nsCOMPtr<nsIChannel> dummyChannel;
    nsCOMPtr<nsIURI> dummyURI;
    rv = serv->NewURI("http://neverneverland.com", nsnull, getter_AddRefs(dummyURI));
    if (NS_FAILED(rv)) return rv;
    rv = NS_NewInputStreamChannel(getter_AddRefs(dummyChannel),
                                  dummyURI,
                                  nsnull,   // inStr
                                  "multipart/x-mixed-replacE;boundary=thisrandomstring",
                                  -1);      // XXX fix contentLength
    if (NS_FAILED(rv)) return rv;


    // setup a listener to receive the converted data. This guy is the end
    // listener in the chain, he wants the fully converted (toType) data.
    // An example of this listener in mozilla would be the DocLoader.
    nsCOMPtr<nsIStreamListener> dataReceiver = new EndListener();

    // setup a listener to push the data into. This listener sits inbetween the
    // unconverted data of fromType, and the final listener in the chain (in this case
    // the dataReceiver.
    nsCOMPtr<nsIStreamListener> converterListener;
    rv = StreamConvService->AsyncConvertData(fromStr.GetUnicode(), toStr.GetUnicode(), dataReceiver, nsnull, getter_AddRefs(converterListener));
    if (NS_FAILED(rv)) return rv;

    // at this point we have a stream listener to push data to, and the one
    // that will receive the converted data. Let's mimic On*() calls and get the conversion
    // going. Typically these On*() calls would be made inside their respective wrappers On*()
    // methods.
    rv = converterListener->OnStartRequest(dummyChannel, nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = SEND_DATA("--thisrandomstring\r\nContent-type: text/html\r\n\r\n<p>Please stand by... <p>\r\n");
    if (NS_FAILED(rv)) return rv;
    
    rv = SEND_DATA("\r\n--thisrandomstring\r\nContent-type: text/html\r\n");
    if (NS_FAILED(rv)) return rv;

    rv = SEND_DATA("Set-Cookie: LASTORDER=bugs.bug_id ; path=/; expires=Sun, 30-Jun-2029 00:00:00 GMT\r\n");
    if (NS_FAILED(rv)) return rv;
    
    rv = SEND_DATA("Set-Cookie: BUGLIST=12012:1750:2023:11112:11948:13288:14960:17164:1485:1582:1596:4528:848:1043:2267:3692:9446:12122:3371:1219:1277:1896:4233:1044:1119:1177:1238:1378:1758:2079:2478:3457:5387:12451:17567:845:1036:1039:1040:1041:1045:1047:1048:1049:1050:1051:1052:1053:1055:1057:1058:1059:1060:1120:1122:1184:1206:1237:1274:1275:1276:1278:1279:1281:1300:1360:1580:1595:1605:1606:1759:1770:1781:1787:1807:1808:1812:1820:1834:1851:1863:1864:1985:2006:2007:2010:2011:2012:2013:2014:2015:2018:2019:2022:2025:2032:2033:2035:2037:2038:2044:2052:2056:2058:2059:2062:2064:2072:2109:2261:2285:2353:2354:2436:2441:2442:2452:2479:2501:2502:2525:2592:2765:2771:2842:2844:2867:2868:2925:2926:2942:2947:2948:2949:2950:2951:2952:2987:2990:2992:2993:3000:3027:3089:3116:3143:3152:3153:3160:3195:3221:3222:3240:3366:3454:3458:3460:3474:3486:4445:4515:4519:5849:6052:6403:8905:9740:9777:9778:9779:9781:9850:12272:12401:12906:2031:3088:850:1042:1046:1141:1414:3013:8044:15992:16934:17418:17519:3950:4580:5850:6518:8032:8088:9024:9236:9593:10176:10273:10296:14310:16586:16848:17645:4387:4426:6357:6519:8045:8071:8565:9013:9474:9738:10268:10269:10274:11960:12217:12398:13140:15315:16490:16585:16624:16636:16936:1038:1413:4042:4050:4092:4234:4510:4529:4572:4615:4831:4833:");
    if (NS_FAILED(rv)) return rv;

    rv = SEND_DATA("4834:5154:5194:5195:5272:5277:5406:5407:5469:5688:5693:5818:5886:6033:6046:6069:6071:6073:6125:6152:6166:6282:6404:6546:6732:6789:6901:6903:7588:7774:7833:7834:7835:8051:8468:8586:8595:8886:8893:8912:8914:9075:9076:9127:9136:9189:9191:9250:9271:9280:9285:9300:9301:9421:9439:9442:9479:9611:9690:9691:9696:9730:9741:9747:9885:9927:10013:10064:10085:10107:10109:10140:10169:10207:10209:10216:10292:10330:10403:10455:10485:10497:10509:10577:10580:10649:10712:10763:10831:11559:11565:11932:11936:12152:12154:12155:12193:12194:12231:12232:12233:12302:12304:12341:12348:12350:12385:12386:12403:12449:12450:12452:12453:12461:12462:12542:12750:12764:12765:12766:12945:12962:12997:12998:13034:13035:13131:13214:13219:13245:13281:12324:14290:13626:14169:14957:14959:14961:15439:15440:15441");
    if (NS_FAILED(rv)) return rv;

    rv = SEND_DATA(":15459:15649:15891:15904:15909:15931:15932:16200:16252:16256:16258:15945:16362:16377:16548:16555:16569:16584:16590:16591:16603:16605:16606:16609:16610:16625:16640:16651:16808:16813:16852:16876:16879:16881:16882:16888:16938:17010:17159:17161:17517:17522:17524:17586:17595:17598:17640:17648:17726:17861:17827:1301:2024:4832:5080:6882:10395:13625:15558:12339:12686:17594:3975:6782:9302:16376:16379:17612:16647:1054:10593:17799:1583:9689:1749:2055:8042\r\n\r\n");
    if (NS_FAILED(rv)) return rv;

    rv = SEND_DATA("TEST DATA--thisrandomstring--");
    if (NS_FAILED(rv)) return rv;    

    // Finish the request.
    rv = converterListener->OnStopRequest(dummyChannel, nsnull, rv, nsnull);
    if (NS_FAILED(rv)) return rv;


#else
    // SYNCRONOUS conversion
    nsIInputStream *convertedData = nsnull;
    rv = StreamConvService->Convert(inputData, fromStr.GetUnicode(), toStr.GetUnicode(), nsnull, &convertedData);
#endif

    NS_RELEASE(convFactSup);
    if (NS_FAILED(rv)) return rv;

    // Enter the message pump to allow the URL load to proceed.
    while ( gKeepRunning ) {
#ifdef WIN32
        MSG msg;

        if (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            gKeepRunning = 0;
    }
#else
#ifdef XP_MAC
    /* Mac stuff is missing here! */
#else
#ifdef XP_OS2
    QMSG qmsg;

    if (WinGetMsg(0, &qmsg, 0, 0, 0))
      WinDispatchMsg(0, &qmsg);
    else
      gKeepRunning = FALSE;
#else
    PLEvent *gEvent;
    rv = gEventQ->GetEvent(&gEvent);
    rv = gEventQ->HandleEvent(gEvent);
#endif /* XP_UNIX */
#endif /* XP_OS2 */
#endif /* !WIN32 */
    }

    return NS_ShutdownXPCOM(NULL);
}
