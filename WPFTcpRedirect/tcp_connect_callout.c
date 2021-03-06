/*++

Copyright (c) Microsoft Corporation. All rights reserved

Abstract:

   Stream Edit Callout Driver Sample.

   This sample demonstrates finding and replacing a string pattern from a
   live TCP stream via the WFP stream API.

   The driver can function in one of the two modes --

      o  Inline Editing where all modification is carried out within the
         WFP ClassifyFn callout function.

      o  Out-of-band (OOB) Editing where all modification is done by a 
         worker thread. (this is the default)

   The mode setting, along with other inspection parameters are configurable
   via the following registry values

  HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\stmedit
      
      o  StringToFind (REG_SZ, default = "rainy")
      o  StringToReplace (REG_SZ, default = "sunny")
      o  InspectionPort (REG_DWORD, default = 5001)
      o  InspectOutbound (REG_DWORD, default = 0)
      o  EditInline (REG_DWORD, default = 0)

   The sample is IP version agnostic. It performs inspection on both IPv4 and
   IPv6 data streams.

   Before experimenting with the sample, please be sure to add an exception for
   the InspectionPort configured to the firewall. 

Environment:

    Kernel mode

--*/

#include "ntddk.h"

#pragma warning(push)
#pragma warning(disable:4201)       // unnamed struct/union

#include "fwpsk.h"

#pragma warning(pop)

#include "fwpmk.h"

#include "inline_edit.h"
#include "tcp_connect_callout.h"

#define INITGUID
#include <guiddef.h>

// 
// Configurable parameters
//

USHORT  configInspectionPort = 80;

// 
// Callout driver keys
//

// e6011cdc-440b-4a6f-8499-6fdb55fb1f92
DEFINE_GUID(
    STREAM_EDITOR_STREAM_CALLOUT_V4,
    0xe6011cdc,
    0x440b,
    0x4a6f,
    0x84, 0x99, 0x6f, 0xdb, 0x55, 0xfb, 0x1f, 0x92
);


// 
// Callout driver global variables
//

HANDLE gEngineHandle;
UINT32 gCalloutIdV4;


PDEVICE_OBJECT gDeviceObject;

#define STREAM_EDITOR_NDIS_OBJ_TAG 'oneS'
#define STREAM_EDITOR_NBL_POOL_TAG 'pneS'
#define STREAM_EDITOR_FLAT_BUFFER_TAG 'bfeS'


DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
   IN  PDRIVER_OBJECT  driverObject,
   IN  PUNICODE_STRING registryPath
   );

DRIVER_UNLOAD DriverUnload;
VOID
DriverUnload(
   IN  PDRIVER_OBJECT driverObject
   );


NTSTATUS
TcpConnectNotify(
   IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
   IN const GUID* filterKey,
   IN const FWPS_FILTER* filter
   )
{
   UNREFERENCED_PARAMETER(notifyType);
   UNREFERENCED_PARAMETER(filterKey);
   UNREFERENCED_PARAMETER(filter);
   return STATUS_SUCCESS;
}

NTSTATUS
RegisterCalloutForLayer(
   IN const GUID* layerKey,
   IN const GUID* calloutKey,
   IN void* deviceObject,
   OUT UINT32* calloutId
   )
/* ++

   This function registers callouts and filters that intercept TCP
   traffic at WFP FWPM_LAYER_STREAM_V4 or FWPM_LAYER_STREAM_V6 layer.

-- */
{
   NTSTATUS status = STATUS_SUCCESS;

   FWPS_CALLOUT sCallout = {0};

   FWPM_FILTER filter = {0};
   FWPM_FILTER_CONDITION filterConditions[1] = {0}; 

   FWPM_CALLOUT mCallout = {0};
   FWPM_DISPLAY_DATA displayData = {0};

   BOOLEAN calloutRegistered = FALSE;

   sCallout.calloutKey = *calloutKey;
   sCallout.classifyFn = TcpConnectClassify;
   sCallout.notifyFn = TcpConnectNotify;

   status = FwpsCalloutRegister(
               deviceObject,
               &sCallout,
               calloutId
               );
   if (!NT_SUCCESS(status))
   {
      goto Exit;
   }
   calloutRegistered = TRUE;

   displayData.name = L"Stream Edit Callout";
   displayData.description = L"Callout that finds and replaces a token from a TCP stream";

   mCallout.calloutKey = *calloutKey;
   mCallout.displayData = displayData;
   mCallout.applicableLayer = *layerKey;
   status = FwpmCalloutAdd(
               gEngineHandle,
               &mCallout,
               NULL,
               NULL
               );

   if (!NT_SUCCESS(status))
   {
      goto Exit;
   }

   filter.layerKey = *layerKey;
   filter.displayData.name = L"Stream Edit Filter";
   filter.displayData.description = L"Filter that finds and replaces a token from a TCP stream";

   filter.action.type = FWP_ACTION_CALLOUT_TERMINATING;
   filter.action.calloutKey = *calloutKey;
   filter.filterCondition = filterConditions;
   filter.numFilterConditions = 1;
   filter.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
   filter.weight.type = FWP_EMPTY; // auto-weight.

   filterConditions[0].fieldKey =  FWPM_CONDITION_IP_REMOTE_PORT;
   filterConditions[0].matchType = FWP_MATCH_EQUAL;
   filterConditions[0].conditionValue.type = FWP_UINT16;
   filterConditions[0].conditionValue.uint16 = configInspectionPort;

   status = FwpmFilterAdd(
               gEngineHandle,
               &filter,
               NULL,
               NULL);
   if (!NT_SUCCESS(status))
   {
      goto Exit;
   }

Exit:

   if (!NT_SUCCESS(status))
   {
      if (calloutRegistered)
      {
         FwpsCalloutUnregisterById(*calloutId);
      }
   }

   return status;
}

NTSTATUS TcpConnectRegisterCallout( IN void* deviceObject )
/* ++

   This function registers dynamic callouts and filters that intercept
   TCP traffic at WFP FWPM_LAYER_STREAM_V4 and FWPM_LAYER_STREAM_V6 
   layer.

   Callouts and filters will be removed during DriverUnload.

-- */
{
   NTSTATUS status = STATUS_SUCCESS;

   BOOLEAN engineOpened = FALSE;
   BOOLEAN inTransaction = FALSE;

   FWPM_SESSION session = {0};

   session.flags = FWPM_SESSION_FLAG_DYNAMIC;

   status = FwpmEngineOpen(
                NULL,
                RPC_C_AUTHN_WINNT,
                NULL,
                &session,
                &gEngineHandle
                );
   if (!NT_SUCCESS(status))
   {
      goto Exit;
   }
   engineOpened = TRUE;

   status = FwpmTransactionBegin(gEngineHandle, 0);
   if (!NT_SUCCESS(status))
   {
      goto Exit;
   }
   inTransaction = TRUE;

   status = RegisterCalloutForLayer(
               &FWPM_LAYER_ALE_CONNECT_REDIRECT_V4,
               &STREAM_EDITOR_STREAM_CALLOUT_V4,
               deviceObject,
               &gCalloutIdV4
               );
   if (!NT_SUCCESS(status))
   {
      goto Exit;
   }

   status = FwpmTransactionCommit(gEngineHandle);
   if (!NT_SUCCESS(status))
   {
      goto Exit;
   }
   inTransaction = FALSE;

Exit:

   if (!NT_SUCCESS(status))
   {
      if (inTransaction)
      {
         FwpmTransactionAbort(gEngineHandle);
      }
      if (engineOpened)
      {
         FwpmEngineClose(gEngineHandle);
         gEngineHandle = NULL;
      }
   }

   return status;
}

void
TcpConnectUnregisterCallout()
{
   FwpmEngineClose(gEngineHandle);
   gEngineHandle = NULL;

   FwpsCalloutUnregisterById(gCalloutIdV4);
}

VOID
DriverUnload(
   IN  PDRIVER_OBJECT driverObject
   )
{

   UNREFERENCED_PARAMETER(driverObject);
   TcpConnectUnregisterCallout();

   IoDeleteDevice(gDeviceObject);
}

NTSTATUS
DriverEntry(
   IN  PDRIVER_OBJECT  driverObject,
   IN  PUNICODE_STRING registryPath
   )
{
   NTSTATUS status = STATUS_SUCCESS;

   UNICODE_STRING deviceName;

   RtlInitUnicodeString(
      &deviceName,
      L"\\Device\\TcpRedirect"
      );

   status = IoCreateDevice(
               driverObject, 
               0, 
               &deviceName, 
               FILE_DEVICE_NETWORK, 
               0, 
               FALSE, 
               &gDeviceObject
               );
   if (!NT_SUCCESS(status))
   {
      goto Exit;
   }

   status = TcpConnectRegisterCallout( gDeviceObject );

   if (!NT_SUCCESS(status))
   {
      goto Exit;
   }

   driverObject->DriverUnload = DriverUnload;

Exit:
   
   if (!NT_SUCCESS(status))
   {
      if (gEngineHandle != NULL)
      {
         TcpConnectUnregisterCallout();
      }

      if (gDeviceObject)
      {
         IoDeleteDevice(gDeviceObject);
      }
   }

   return status;
}



