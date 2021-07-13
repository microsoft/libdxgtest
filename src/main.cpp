/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License.
 *
 * WDDM kernel API test
 *
 */

#include <wsl/winadapter.h>
#include <dxg/d3dkmthk.h>
#include <stdio.h>

#include "sys/mman.h"
#include <sys/eventfd.h>
#include <locale.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <wsl/winadapter.h>
#include <dxg/d3dkmthk.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <time.h>
#define STATUS_ABANDONED       ((NTSTATUS)0x00000080L)
#define DXGI_SHARED_RESOURCE_READ    ( 0x80000000L )
#define DXGI_SHARED_RESOURCE_WRITE    ( 0x00000001L )

const UINT PAGE_SIZE         = 4096;
const UINT PAGE_SIZE_4K     = PAGE_SIZE;
const UINT PAGE_SIZE_64K     = PAGE_SIZE * 16;
#define ARRAY_SIZE(a)  sizeof(a)/sizeof(a[0])
NTSTATUS TestAll(const char*);
NTSTATUS EnumAdapters();
NTSTATUS ExecuteTest(const char* s);

extern void PrintMessage(UINT Level, const char* format, ...);
extern void PrintMessageSkipped(const char* format, ...);

struct DXGK_TEST {
	NTSTATUS (*pFunction)(const char* Name);
	const char* Name;
};

#define DEFINE_DXGKTEST(TestName)	\
	{TestName, #TestName},

extern DXGK_TEST Tests[];
extern UINT g_VerboseLevel;
extern UINT g_AdapterIndex;

struct ADAPTER_DESC
{
    bool HwSchEnabled;
    bool HwSchSupported;
    D3DKMT_ADAPTERTYPE AdapterType;
    D3DKMT_DEVICE_IDS DeviceId;
};

const UINT MAX_ADAPTERS                         = 16;
D3DKMT_HANDLE g_hAdapter                        = 0;
UINT g_NumAdapters                              = 0;
D3DKMT_ADAPTERINFO Adapters[MAX_ADAPTERS]       = {};
D3DKMT_DRIVER_DESCRIPTION DriverDescriptor[16]  = {};
ADAPTER_DESC g_AdapterDesc[MAX_ADAPTERS]        = {};
UINT g_AdapterIndex                             = 0;
int  g_SoftGpuAdapterIndex                      = -1;
UINT g_SoftGpuStartNodeIndex                    = 0;
bool g_Step                                     = false;
UINT g_VerboseLevel                             = 0;

struct PAGINGQUEUE {
    D3DKMT_HANDLE     handle;
    D3DKMT_HANDLE     syncobject;
    UINT64*           fenceva;
};

void FunctionEnter(const char* FunctionName)
{
    PrintMessage(2, "%s", FunctionName);
}
void FunctionExit(const char* FunctionName, NTSTATUS Status)
{
    if (!NT_SUCCESS(Status))
            PrintMessage(0, "%s FAILED %x", FunctionName, Status);
    else
    if (Status == STATUS_ABANDONED)
        PrintMessage(0, "%s SKIPPED", FunctionName);
    else
        PrintMessage(1, "%s SUCCESS", FunctionName);
}
VOID PrintString(WCHAR* s, char* output)
{
    int i = 0;
    char tmp[8];
    strcat(output, "'");
    while (s[i] != 0) {
        sprintf(tmp, "%c", s[i]);
        strcat(output, tmp);
        i++;
    }
    strcat(output, "'");
}

//-------------------------------------------------------------------------------
D3DKMT_HANDLE CreateDevice(D3DKMT_HANDLE hAdapter)
{
    D3DKMT_CREATEDEVICE Args = { 0 };
    Args.hAdapter = hAdapter;
    NTSTATUS Status = D3DKMTCreateDevice(&Args);
    if (!NT_SUCCESS(Status))
    {
        PrintMessage(2, "Failed to create device: %x", Status);
    }
    return Args.hDevice;
}
//-------------------------------------------------------------------------------
D3DKMT_HANDLE CreateContext(D3DKMT_HANDLE hDevice,
    UINT NodeOrdinal = 0,
    UINT PhysicalAdapterIndex = 0,
    D3DKMT_CLIENTHINT ClientHint = D3DKMT_CLIENTHINT_CDD,
    BOOLEAN HwScheduling = FALSE)
{
    D3DKMT_CREATECONTEXTVIRTUAL Args = { 0 };
    Args.hDevice = hDevice;
    Args.NodeOrdinal = NodeOrdinal;
    Args.EngineAffinity = 1 << PhysicalAdapterIndex;
    Args.ClientHint = ClientHint;
    if (HwScheduling)
        Args.Flags.HwQueueSupported = 1;
    NTSTATUS Status = D3DKMTCreateContextVirtual(&Args);
    if (!NT_SUCCESS(Status))
    {
        PrintMessage(2, "Failed to create context: %x", Status);
    }
    return Args.hContext;
}
//-----------------------------------------------------------------------------
NTSTATUS DestroyDevice(D3DKMT_HANDLE hDevice)
{
    D3DKMT_DESTROYDEVICE Args = { 0 };
    Args.hDevice = hDevice;
    NTSTATUS Status = D3DKMTDestroyDevice(&Args);
    if (!NT_SUCCESS(Status))
    {
        PrintMessage(2, "Failed to destroy device");
    }
    return Status;
}
//-----------------------------------------------------------------------------
NTSTATUS DestroyContext(D3DKMT_HANDLE hContext)
{
    D3DKMT_DESTROYCONTEXT Args = { 0 };
    Args.hContext = hContext;
    NTSTATUS Status = D3DKMTDestroyContext(&Args);
    if (!NT_SUCCESS(Status))
    {
        PrintMessage(2, "Failed to destroy context");
    }
    return Status;
}
///////////////////////////////////////////////////////////////////////////////
static void* thread_start(void* a)
{
    D3DKMT_OPENADAPTERFROMLUID args = {};
    args.AdapterLuid = Adapters[0].AdapterLuid;
    NTSTATUS Status = STATUS_SUCCESS;
    Status = D3DKMTOpenAdapterFromLuid(&args);
    if (!NT_SUCCESS(Status)) {
        PrintMessage(1, "D3DKMTOpenAdapterFromLuid failed %x",Status);
    } else {
        D3DKMT_CLOSEADAPTER args2 = {};
        PrintMessage(2, "Adapter opened: %x", args.hAdapter);
        args2.hAdapter = args.hAdapter;
        Status = D3DKMTCloseAdapter(&args2);
        if (!NT_SUCCESS(Status))
            PrintMessage(1, "D3DKMTCloseAdapter failed %x", Status);
    }
    PrintMessage(2, "Thread finished %x", Status);
    *(NTSTATUS*)a = Status;
    return NULL;
}
//-------------------------------------------------------------
NTSTATUS TestCreateThread(const char* FunctionName)
{
    int ret;
    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    FunctionEnter(FunctionName);

    pthread_t thread_id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    ret = pthread_create(&thread_id, &attr, thread_start, &Status);
    if (ret) {
        PrintMessage(1, "Failed to create thread: %x", ret);
        goto cleanup;
    }
    ret = pthread_join(thread_id, NULL);
    if (ret) {
        PrintMessage(1, "pthread_join failed: %x", ret);
        goto cleanup;
    }

cleanup:
    FunctionExit(FunctionName, Status);
    return Status;
}
//-----------------------------------------------------------------------------
NTSTATUS TestOpenAdapterFromLuid(const char* FunctionName)
{
    D3DKMT_OPENADAPTERFROMLUID args = {};
    NTSTATUS Status = STATUS_SUCCESS;
    FunctionEnter(FunctionName);
    args.AdapterLuid = Adapters[0].AdapterLuid;

    Status = D3DKMTOpenAdapterFromLuid(&args);
    if (!NT_SUCCESS(Status)) {
        PrintMessage(1, "D3DKMTOpenAdapterFromLuid failed %x",Status);
    } else {
        D3DKMT_CLOSEADAPTER args2 = {};
        PrintMessage(2, "Adapter opened: %x", args.hAdapter);
        args2.hAdapter = args.hAdapter;
        Status = D3DKMTCloseAdapter(&args2);
        if (!NT_SUCCESS(Status))
            PrintMessage(1, "D3DKMTCloseAdapter failed %x", Status);
    }
    FunctionExit(FunctionName, Status);
    return Status;
}
//-----------------------------------------------------------------------------
NTSTATUS TestCreateDevice(const char* FunctionName)
{
    int i;
    D3DKMT_HANDLE hAdapter = g_hAdapter;
    NTSTATUS Status = STATUS_SUCCESS;
    FunctionEnter(FunctionName);
    for (i = 0; i < 5; i++ ) {
        D3DKMT_HANDLE hDevice = CreateDevice(hAdapter);
        if (hDevice) {
            DestroyDevice(hDevice);
        } else {
            Status = STATUS_UNSUCCESSFUL;
            break;
        }
    }
    FunctionExit(FunctionName, Status);
    return Status;
}
//-----------------------------------------------------------------------------
NTSTATUS TestCreateContext(const char* FunctionName)
{
    D3DKMT_HANDLE hDevice;
    D3DKMT_HANDLE hAdapter = g_hAdapter;
    NTSTATUS Status = STATUS_SUCCESS;
    FunctionEnter(FunctionName);
    hDevice = CreateDevice(hAdapter);
    if (hDevice) {
        D3DKMT_HANDLE hContext = CreateContext(hDevice, 0, 0, D3DKMT_CLIENTHINT_CDD, g_AdapterDesc[g_AdapterIndex].HwSchEnabled);
        if (hContext) {
            DestroyContext(hContext);
        } else {
            Status = STATUS_UNSUCCESSFUL;
        }
        DestroyDevice(hDevice);
    }
    FunctionExit(FunctionName, Status);
    return Status;
}
//-----------------------------------------------------------------------------
NTSTATUS EnumAdapters()
{
    D3DKMT_ENUMADAPTERS3 Args = {};
    Args.NumAdapters = MAX_ADAPTERS;
    Args.pAdapters = Adapters;
    Args.Filter.IncludeComputeOnly = 1;
    NTSTATUS Status = D3DKMTEnumAdapters3(&Args);
    if (!NT_SUCCESS(Status))
    {
        PrintMessage(1, "D3DKMTEnumAdapters3 failed: %x", Status);
        return STATUS_UNSUCCESSFUL;
    }
    PrintMessage(0, "D3DKMTEnumAdapters3 returned %d adapters:", Args.NumAdapters);
    g_NumAdapters = Args.NumAdapters;
    for (UINT i = 0; i < g_NumAdapters; i++)
    {
        D3DKMT_ADAPTERTYPE AdapterType;
        bool SoftGpu = false;
        {
            D3DKMT_QUERYADAPTERINFO Args1 = {};
            Args1.hAdapter = Adapters[i].hAdapter;
            Args1.Type = KMTQAITYPE_ADAPTERTYPE_RENDER;
            Args1.pPrivateDriverData = &AdapterType;
            Args1.PrivateDriverDataSize = sizeof(D3DKMT_ADAPTERTYPE);
            Status = D3DKMTQueryAdapterInfo(&Args1);
            if (Status != 0)
            {
                PrintMessage(1, "D3DKMTQueryAdapterInfo failed");
                return STATUS_UNSUCCESSFUL;
            }
            g_AdapterDesc[i].AdapterType = AdapterType;
        }
        {
            D3DKMT_QUERY_DEVICE_IDS Args = {};
            D3DKMT_QUERYADAPTERINFO Args1 = {};
            Args1.hAdapter = Adapters[i].hAdapter;
            Args1.Type = KMTQAITYPE_PHYSICALADAPTERDEVICEIDS;
            Args1.pPrivateDriverData = &Args;
            Args1.PrivateDriverDataSize = sizeof(Args);
            Status = D3DKMTQueryAdapterInfo(&Args1);
            if (NT_SUCCESS(Status))
            {
                g_AdapterDesc[i].DeviceId = Args.DeviceIds;
            } else {
                PrintMessage(1, "KMTQAITYPE_PHYSICALADAPTERDEVICEIDS failed");
                return STATUS_UNSUCCESSFUL;
            }
        }
        {
            D3DKMT_WDDM_2_7_CAPS AdapterCaps = {};
            D3DKMT_QUERYADAPTERINFO Args1 = {};
            Args1.hAdapter = Adapters[i].hAdapter;
            Args1.Type = KMTQAITYPE_WDDM_2_7_CAPS;
            Args1.pPrivateDriverData = &AdapterCaps;
            Args1.PrivateDriverDataSize = sizeof(AdapterCaps);
            Status = D3DKMTQueryAdapterInfo(&Args1);
            if (NT_SUCCESS(Status))
            {
                g_AdapterDesc[i].HwSchEnabled = AdapterCaps.HwSchEnabled;
                g_AdapterDesc[i].HwSchSupported = AdapterCaps.HwSchSupported;
            }
        }
        {
            D3DKMT_QUERYADAPTERINFO Args1 = {};
            Args1.hAdapter = Adapters[i].hAdapter;
            Args1.Type = KMTQAITYPE_DRIVER_DESCRIPTION;
            Args1.pPrivateDriverData = &DriverDescriptor[i];
            Args1.PrivateDriverDataSize = sizeof(DriverDescriptor[0]);
            Status = D3DKMTQueryAdapterInfo(&Args1);
            if (Status != 0)
            {
                PrintMessage(1, "KMTQAITYPE_DRIVER_DESCRIPTION failed: %x", Status);
                return STATUS_UNSUCCESSFUL;
            }
        }
        if (AdapterType.RenderSupported &&
            g_AdapterDesc[i].DeviceId.VendorID == 0x1414 &&
            g_AdapterDesc[i].DeviceId.DeviceID == 0x88)
        {
            SoftGpu = true;
        }
        char s[512];
        sprintf(s, "%d Handle: 0x%x, Luid: 0x%x-%x  VendorId: 0x%x DeviceId:0x%x Num sources: %d ",
            i,
            Adapters[i].hAdapter,
            Adapters[i].AdapterLuid.HighPart,
            Adapters[i].AdapterLuid.LowPart,
            g_AdapterDesc[i].DeviceId.VendorID,
            g_AdapterDesc[i].DeviceId.DeviceID,
            Adapters[i].NumOfSources);
        PrintString(DriverDescriptor[i].DriverDescription, s);
        if (AdapterType.Paravirtualized)
            strcat(s, " gpu-pv");
        if (AdapterType.RenderSupported)
            strcat(s, " render");
        if (AdapterType.DisplaySupported)
            strcat(s, " display");
        if (AdapterType.PostDevice)
            strcat(s, " post");
        if (SoftGpu)
            strcat(s, " softgpu");
        if (AdapterType.ComputeOnly)
            strcat(s, " compute_only");
        if (AdapterType.SoftwareDevice)
            strcat(s, " software");
        if (AdapterType.Detachable)
            strcat(s, " detachable");
        if (g_AdapterDesc[i].HwSchEnabled)
            strcat(s, " hwsch_enabled");
        else
        if (g_AdapterDesc[i].HwSchSupported)
            strcat(s, " hwsch_support");

        PrintMessage(0, s);
    }
    g_hAdapter = Adapters[g_AdapterIndex].hAdapter;
    PrintMessage(0, "--------------------------------------------------------");
    PrintMessage(0, "Current adapter: ");
    char s[512];
    s[0] = 0;
    PrintString(DriverDescriptor[g_AdapterIndex].DriverDescription, s);
    PrintMessage(0, s);
    PrintMessage(0, "--------------------------------------------------------");
    return STATUS_SUCCESS;
}
//-----------------------------------------------------------------------------
DXGK_TEST Tests[] = {
    DEFINE_DXGKTEST(TestAll)
    DEFINE_DXGKTEST(TestOpenAdapterFromLuid)
    DEFINE_DXGKTEST(TestCreateDevice)
    DEFINE_DXGKTEST(TestCreateThread)
    DEFINE_DXGKTEST(TestCreateContext)
};
//-----------------------------------------------------------------------------
NTSTATUS ExecuteTest(const char* s)
{
    for (unsigned int i = 0; i < ARRAY_SIZE(Tests); i++) {
        if (strcasecmp(s, Tests[i].Name) == 0)
            return Tests[i].pFunction(Tests[i].Name);
    }
    return STATUS_UNSUCCESSFUL;
}
//-----------------------------------------------------------------------------
void PrintMessage(UINT Level, const char* format, ...)
{
    if (g_VerboseLevel >= Level)
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}
void PrintMessageSkipped(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}
//-----------------------------------------------------------------------------
NTSTATUS TestAll(const char*)
{
    for (unsigned int i = 1; i < ARRAY_SIZE(Tests); i++) {
        Tests[i].pFunction(Tests[i].Name);
        if (g_Step) {
            PrintMessage(0, "Press any key to continue...");
            getchar();
        }
    }
    return STATUS_SUCCESS;
}
//-----------------------------------------------------------------------------
void PrintUsage()
{
    PrintMessage(0, "Usage: dxgktest [options]");
    PrintMessage(0, "Options:");
    PrintMessage(0, "  -adapter <index>              - pick the adapter with the given index");
    PrintMessage(0, "  -v <level>                    - set verbose level for printing messages");
    PrintMessage(0, "  -tn <test name>               - execute a test by name");
    PrintMessage(0, "  -t <test index>               - execute a test by index");
    PrintMessage(0, "  -step                         - stop after each test");
    PrintMessage(0, "Available tests:");
    for (unsigned int i = 0; i < ARRAY_SIZE(Tests); i++) {
        PrintMessage(0, "%2d %s", i, Tests[i].Name);
    }
}
//------------------------------------------------------------------------------
BOOLEAN ParseCommandLine(int argc, char* argv[])
{
    int TestIndex = -1;
    char* TestName = NULL;
    if(argc == 1)
    {
        PrintUsage();
    }
    else
    for (int i = 1; i < argc; i++)
    {
        if (strcasecmp(argv[i], "-?") == 0) {
            if (!NT_SUCCESS(EnumAdapters())) {
                PrintMessage(0, "Failed to enumerate adapters");
                return FALSE;
            }
            if (g_NumAdapters == 0) {
                PrintMessage(0, "No GRFX adapters are found");
                return FALSE;
            }
            PrintUsage();
            return TRUE;
        }
        else if (strcasecmp(argv[i], "-step") == 0) {
            g_Step = true;
        }
        else if (strcasecmp(argv[i], "-adapter") == 0) {
            if (argc - i < 1)
            {
                PrintMessage(1, "-adapter requires adapter index");
                return FALSE;
            }
            g_AdapterIndex = atoi(argv[++i]);
        }
        else if (strcasecmp(argv[i], "-v") == 0) {
            if (argc - i < 1)
            {
                PrintMessage(0, "-verbose requires level index");
                return FALSE;
            }
            g_VerboseLevel = atoi(argv[++i]);
        }
        else if (strcasecmp(argv[i], "-tn") == 0) {
            if (argc - i < 1)
            {
                PrintMessage(0, "-tn requires a test name");
                return FALSE;
            }
            if (!NT_SUCCESS(EnumAdapters())) {
                PrintMessage(0, "Failed to enumerate adapters");
                return FALSE;
            }
            if (g_NumAdapters == 0) {
                PrintMessage(0, "No GRFX adapters are found");
                return FALSE;
            }
            TestName = argv[++i];
        }
        else if (strcasecmp(argv[i], "-t") == 0) {
            if (argc - i < 1)
            {
                PrintMessage(0, "-ti requires test index");
                return FALSE;
            }
            TestIndex = atoi(argv[++i]);
            if (TestIndex < 0 || TestIndex >= (int)(ARRAY_SIZE(Tests))) {
                PrintMessage(0, "Invalid test index: %d", TestIndex);
                return FALSE;
            }
            if (!NT_SUCCESS(EnumAdapters())) {
                PrintMessage(0, "Failed to enumerate adapters");
                return FALSE;
            }
            if (g_NumAdapters == 0) {
                PrintMessage(0, "No GRFX adapters are found");
                return FALSE;
            }
        }
        else {
            PrintMessage(0, "Unknown command: %s", argv[i]);
            PrintUsage();
            return FALSE;
        }
    }
    if (TestName)
        ExecuteTest(TestName);
    if (TestIndex != -1)
        Tests[TestIndex].pFunction(Tests[TestIndex].Name);

    return TRUE;
}
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    NTSTATUS status;

    setlocale(LC_ALL,"");

    if (ParseCommandLine(argc, argv) == FALSE) {
        PrintMessage(0, "Failed to parse command line");
        return 0;
    }

    for (UINT i = 0; i < g_NumAdapters; i++) {
        D3DKMT_CLOSEADAPTER args2 = {Adapters[i].hAdapter};
        status = D3DKMTCloseAdapter(&args2);
        if (!NT_SUCCESS(status)) {
            PrintMessage(0, "Failed to close adapter %x %x", Adapters[i].hAdapter, status);
        }
    }

    return 0;
}
