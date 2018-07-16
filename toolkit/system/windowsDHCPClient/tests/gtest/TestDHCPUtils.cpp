/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "DHCPUtils.h"
#include "gtest/gtest.h"
#include "nsString.h"
#include "nsWindowsDHCPClient.h"

using namespace mozilla::toolkit::system::windowsDHCPClient;

class WindowsNetworkFunctionsMock : public WindowsNetworkFunctionsWrapper {

  public:
    WindowsNetworkFunctionsMock():mAddressesToReturn(nullptr) {
      memset(mOptions, 0, sizeof(char*) * 256);
    }

    ULONG GetAdaptersAddressesWrapped(
      _In_    ULONG                 Family,
      _In_    ULONG                 Flags,
      _In_    PVOID                 Reserved,
      _Inout_ PIP_ADAPTER_ADDRESSES AdapterAddresses,
      _Inout_ PULONG                SizePointer
    ){
      if (*SizePointer < sizeof(*mAddressesToReturn)){
        *SizePointer = sizeof(*mAddressesToReturn);
        return ERROR_BUFFER_OVERFLOW;
      }

      *SizePointer = sizeof(*mAddressesToReturn);
      memcpy(AdapterAddresses, mAddressesToReturn,
                    *SizePointer);
      return 0;
    }

    DWORD DhcpRequestParamsWrapped(
      _In_    DWORD                 Flags,
      _In_    LPVOID                Reserved,
      _In_    LPWSTR                AdapterName,
      _In_    LPDHCPCAPI_CLASSID    ClassId,
      _In_    DHCPCAPI_PARAMS_ARRAY SendParams,
      _Inout_ DHCPCAPI_PARAMS_ARRAY RecdParams,
      _In_    LPBYTE                Buffer,
      _Inout_ LPDWORD               pSize,
      _In_    LPWSTR                RequestIdStr
    )
    {
      mLastRequestedNetworkAdapterName.Assign(AdapterName);

      if (mOptions[RecdParams.Params[0].OptionId] == nullptr) {
        RecdParams.Params[0].nBytesData = 0;
      }
      else {
        RecdParams.Params[0].Data = Buffer;
        size_t lengthOfValue = strlen(mOptions[RecdParams.Params[0].OptionId]);
        if (*pSize > lengthOfValue) {
          memcpy(Buffer, mOptions[RecdParams.Params[0].OptionId], lengthOfValue);
          RecdParams.Params[0].nBytesData = lengthOfValue;
        } else {
          *pSize = lengthOfValue;
          return ERROR_MORE_DATA;
        }
      }
      return 0;
    }

    void
    AddAdapterAddresses(IP_ADAPTER_ADDRESSES& aAddressesToAdd)
    {
      if (mAddressesToReturn == nullptr) {
        mAddressesToReturn = &aAddressesToAdd;
        return;
      }
      IP_ADAPTER_ADDRESSES* tail = mAddressesToReturn;

      while (tail->Next != nullptr) {
        tail = tail->Next;
      }
      tail->Next = &aAddressesToAdd;
    }

    void
    SetDHCPOption(uint8_t option, char* value)
    {
      mOptions[option] = value;
    }

    nsString
    GetLastRequestedNetworkAdapterName()
    {
      return mLastRequestedNetworkAdapterName;
    }

  private:
    IP_ADAPTER_ADDRESSES* mAddressesToReturn = nullptr;
    char* mOptions[256];
    nsString mLastRequestedNetworkAdapterName;
};

class TestDHCPUtils : public ::testing::Test {
  protected:
    RefPtr<WindowsNetworkFunctionsMock> mMockWindowsFunctions;
    nsCString mDefaultAdapterName;

    virtual void
    SetUp()
    {
      mMockWindowsFunctions = new WindowsNetworkFunctionsMock();
      mDefaultAdapterName.AssignLiteral("my favourite network adapter");
    }

    void
    Given_DHCP_Option_Is(uint8_t option, char* value)
    {
      mMockWindowsFunctions.get()->SetDHCPOption(option, value);
    }

    void
    Given_Network_Adapter_Called(
      IP_ADAPTER_ADDRESSES& adapterAddresses,
      char* adapterName)
    {
      adapterAddresses.AdapterName = adapterName;
      adapterAddresses.Next = nullptr;
      adapterAddresses.Dhcpv4Server.iSockaddrLength = 0;
      adapterAddresses.Dhcpv6Server.iSockaddrLength = 0;
      AddAdapterAddresses(adapterAddresses);
    }

    void
    Given_Network_Adapter_Supports_DHCP_V4(IP_ADAPTER_ADDRESSES& adapterAddresses)
    {
      adapterAddresses.Dhcpv4Server.iSockaddrLength = 4;
    }

    void
    Given_Network_Adapter_Supports_DHCP_V6(IP_ADAPTER_ADDRESSES& adapterAddresses)
    {
      adapterAddresses.Dhcpv6Server.iSockaddrLength = 12;
    }

    void
    Given_Network_Adapter_Has_Operational_Status(
        IP_ADAPTER_ADDRESSES& adapterAddresses,
        IF_OPER_STATUS operStatus)
    {
      adapterAddresses.OperStatus = operStatus;
    }

  private:
    void
    AddAdapterAddresses(IP_ADAPTER_ADDRESSES& aAddressToAdd)
    {
      mMockWindowsFunctions.get()->AddAdapterAddresses(aAddressToAdd);
    }

};

// following class currently just distinguishes tests of nsWindowsDHCPClient from
// tests of DHCPUtils.
class TestNsWindowsDHCPClient : public TestDHCPUtils { };


TEST_F(TestDHCPUtils, TestGetAdaptersAddresses)
{
  IP_ADAPTER_ADDRESSES adapterAddresses = {};
  Given_Network_Adapter_Called(adapterAddresses, "my favourite network adapter");
  Given_Network_Adapter_Supports_DHCP_V4(adapterAddresses);
  Given_Network_Adapter_Has_Operational_Status(adapterAddresses, IfOperStatusUp);

  nsCString networkAdapterName;

  ASSERT_EQ(NS_OK, GetActiveDHCPNetworkAdapterName(networkAdapterName, mMockWindowsFunctions));

  ASSERT_STREQ(networkAdapterName.Data(), "my favourite network adapter");
}

TEST_F(TestDHCPUtils, TestGetAdaptersAddressesNoAvailableNetworks)
{
  IP_ADAPTER_ADDRESSES adapterAddresses = {};
  Given_Network_Adapter_Called(adapterAddresses, "my favourite network adapter");
  Given_Network_Adapter_Supports_DHCP_V4(adapterAddresses);
  Given_Network_Adapter_Has_Operational_Status(adapterAddresses, IfOperStatusDown);

  nsCString networkAdapterName;
  ASSERT_EQ(NS_ERROR_NOT_AVAILABLE, GetActiveDHCPNetworkAdapterName(networkAdapterName, mMockWindowsFunctions));

  ASSERT_STREQ(networkAdapterName.Data(), "");
}

TEST_F(TestDHCPUtils, TestGetAdaptersAddressesNoNetworksWithDHCP)
{
  IP_ADAPTER_ADDRESSES adapterAddresses = {};
  Given_Network_Adapter_Called(adapterAddresses, "my favourite network adapter");
  Given_Network_Adapter_Has_Operational_Status(adapterAddresses, IfOperStatusUp);

  nsCString networkAdapterName;
  ASSERT_EQ(NS_ERROR_NOT_AVAILABLE, GetActiveDHCPNetworkAdapterName(networkAdapterName, mMockWindowsFunctions));

  ASSERT_STREQ(networkAdapterName.Data(), "");
}

TEST_F(TestDHCPUtils, TestGetAdaptersAddressesSecondNetworkIsAvailable)
{
  IP_ADAPTER_ADDRESSES adapterAddresses = {};
  Given_Network_Adapter_Called(adapterAddresses, "my favourite network adapter");
  Given_Network_Adapter_Supports_DHCP_V4(adapterAddresses);
  Given_Network_Adapter_Has_Operational_Status(adapterAddresses, IfOperStatusDown);


  IP_ADAPTER_ADDRESSES secondAdapterAddresses = {};
  Given_Network_Adapter_Called(secondAdapterAddresses, "my second favourite network adapter");
  Given_Network_Adapter_Supports_DHCP_V6(secondAdapterAddresses);
  Given_Network_Adapter_Has_Operational_Status(secondAdapterAddresses, IfOperStatusUp);

  nsCString networkAdapterName;
  ASSERT_EQ(NS_OK, GetActiveDHCPNetworkAdapterName(networkAdapterName, mMockWindowsFunctions));

  ASSERT_STREQ(networkAdapterName.Data(), "my second favourite network adapter");
}

TEST_F(TestDHCPUtils, TestGetOption)
{

  char* pacURL = "http://pac.com";
  Given_DHCP_Option_Is(1, "My network option");
  Given_DHCP_Option_Is(252, pacURL);

  std::vector<char> optionValue(255, *"originalValue originalValue");
  memcpy(optionValue.data(), "originalValue originalValue", strlen("originalValue originalValue") + 1);

  uint32_t size = 255;

  nsresult retVal = RetrieveOption(mDefaultAdapterName, 252, optionValue, &size, mMockWindowsFunctions);

  ASSERT_EQ(strlen(pacURL), size);
  ASSERT_STREQ("http://pac.comoriginalValue", optionValue.data());
  ASSERT_EQ(NS_OK, retVal);
}

TEST_F(TestDHCPUtils, TestGetAbsentOption)
{
  std::vector<char> optionValue(255);
  uint32_t size = 256;
  memcpy(optionValue.data(), "originalValue", strlen("originalValue") + 1);

  nsresult retVal = RetrieveOption(mDefaultAdapterName, 252, optionValue, &size, mMockWindowsFunctions);

  ASSERT_EQ(0, size);
  ASSERT_EQ(NS_ERROR_NOT_AVAILABLE, retVal);
}

TEST_F(TestDHCPUtils, TestGetTooLongOption)
{
  Given_DHCP_Option_Is(252, "http://pac.com");

  std::vector<char> optionValue(255);
  memcpy(optionValue.data(), "originalValue", strlen("originalValue") + 1);
  uint32_t size = 4;
  nsresult retVal = RetrieveOption(mDefaultAdapterName, 252, optionValue, &size, mMockWindowsFunctions);

  ASSERT_STREQ("httpinalValue", optionValue.data());
  ASSERT_EQ(NS_ERROR_LOSS_OF_SIGNIFICANT_DATA, retVal);
  ASSERT_EQ(strlen("http://pac.com"), size);
}

TEST_F(TestNsWindowsDHCPClient, TestGettingOptionThroughNSWindowsDHCPClient)
{
  IP_ADAPTER_ADDRESSES adapterAddresses = {};
  Given_Network_Adapter_Called(adapterAddresses, "my favourite network adapter");
  Given_Network_Adapter_Supports_DHCP_V4(adapterAddresses);
  Given_Network_Adapter_Has_Operational_Status(adapterAddresses, IfOperStatusUp);
  Given_DHCP_Option_Is(252, "http://pac.com");

  nsCString optionValue;
  nsCOMPtr<nsIDHCPClient> dhcpClient = new nsWindowsDHCPClient(mMockWindowsFunctions);
  nsresult retVal = dhcpClient->GetOption(252, optionValue);

  ASSERT_STREQ("http://pac.com", optionValue.Data());
  ASSERT_STREQ(L"my favourite network adapter", mMockWindowsFunctions->GetLastRequestedNetworkAdapterName().Data());
  ASSERT_EQ(NS_OK, retVal);
}

TEST_F(TestNsWindowsDHCPClient, TestGettingOptionThroughNSWindowsDHCPClientWhenNoAvailableNetworkAdapter)
{
  IP_ADAPTER_ADDRESSES adapterAddresses = {};
  Given_Network_Adapter_Called(adapterAddresses, "my favourite network adapter");
  Given_Network_Adapter_Has_Operational_Status(adapterAddresses, IfOperStatusDown);
  Given_DHCP_Option_Is(252, "http://pac.com");

  nsCString optionValue;
  nsCOMPtr<nsIDHCPClient> dhcpClient = new nsWindowsDHCPClient(mMockWindowsFunctions);
  nsresult retVal = dhcpClient->GetOption(252, optionValue);

  ASSERT_STREQ("", optionValue.Data());
  ASSERT_EQ(NS_ERROR_NOT_AVAILABLE, retVal);
}

TEST_F(TestNsWindowsDHCPClient, TestGettingAbsentOptionThroughNSWindowsDHCPClient)
{
  IP_ADAPTER_ADDRESSES adapterAddresses = {};
  Given_Network_Adapter_Called(adapterAddresses, "my favourite network adapter");
  Given_Network_Adapter_Supports_DHCP_V6(adapterAddresses);
  Given_Network_Adapter_Has_Operational_Status(adapterAddresses, IfOperStatusUp);

  nsCString optionValue;
  nsCOMPtr<nsIDHCPClient> dhcpClient = new nsWindowsDHCPClient(mMockWindowsFunctions);
  nsresult retVal = dhcpClient->GetOption(252, optionValue);

  ASSERT_STREQ("", optionValue.Data());
  ASSERT_EQ(NS_ERROR_NOT_AVAILABLE, retVal);
}