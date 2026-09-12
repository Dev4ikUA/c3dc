#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/GraphicsOutput.h>

#define AMD3DC_CORE_COUNT        2
#define AMD3DC_SEGMENT_COUNT     4
#define AMD3DC_DISABLED_SEGMENTS 1

#define AMD3DC_HBM3_GIB          24ULL

#define AMD3DC_BG_COLOR \
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL_FROM_RGB(8, 12, 20)

#define AMD3DC_ACCENT_COLOR \
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL_FROM_RGB(40, 120, 220)

static EFI_STATUS
AMD3DCDrawBackground(
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop
    )
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color;

    Color.Blue = 20;
    Color.Green = 12;
    Color.Red = 8;
    Color.Reserved = 0;

    return Gop->Blt(
        Gop,
        &Color,
        EfiBltVideoFill,
        0,
        0,
        0,
        0,
        Gop->Mode->Info->HorizontalResolution,
        Gop->Mode->Info->VerticalResolution,
        0
    );
}

static EFI_STATUS
AMD3DCDrawBar(
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop,
    UINT32 X,
    UINT32 Y,
    UINT32 Width,
    UINT32 Height
    )
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color;

    Color.Blue = 220;
    Color.Green = 120;
    Color.Red = 40;
    Color.Reserved = 0;

    return Gop->Blt(
        Gop,
        &Color,
        EfiBltVideoFill,
        0,
        0,
        X,
        Y,
        Width,
        Height,
        0
    );
}

EFI_STATUS
EFIAPI
AMD3DCEfiEntryPoint(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE* SystemTable
    )
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop;

    (VOID)ImageHandle;
    (VOID)SystemTable;

    Status = gBS->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (VOID**)&Gop
    );

    if (EFI_ERROR(Status))
    {
        return Status;
    }

    /*
     * Use the emulator GOP mode selected by EmulatorPkg.
     */
    Status = AMD3DCDrawBackground(Gop);

    if (EFI_ERROR(Status))
    {
        return Status;
    }

    /*
     * Decorative EFI splash bar.
     */
    if (Gop->Mode->Info->HorizontalResolution >= 640 &&
        Gop->Mode->Info->VerticalResolution >= 480)
    {
        AMD3DCDrawBar(
            Gop,
            32,
            32,
            Gop->Mode->Info->HorizontalResolution - 64,
            4
        );
    }

    /*
     * EFI text console.
     */
    if (gST->ConOut != NULL)
    {
        gST->ConOut->ClearScreen(gST->ConOut);

        gST->ConOut->SetAttribute(
            gST->ConOut,
            EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK
        );

        Print(L"\r\n");
        Print(L"============================================================\r\n");
        Print(L"                       AMD 3DC EFI\r\n");
        Print(L"                   Cellarium 3DC Platform\r\n");
        Print(L"============================================================\r\n");
        Print(L"\r\n");

        Print(L"Processor architecture : 64-bit\r\n");
        Print(L"Byte order             : Big Endian\r\n");
        Print(L"Processor cores        : %u\r\n",
            AMD3DC_CORE_COUNT);

        Print(L"\r\n");

        Print(L"Cache hierarchy        : L1 / L2 / L3 / L4\r\n");
        Print(L"HBM3 logical capacity  : %llu GiB\r\n",
            AMD3DC_HBM3_GIB);

        Print(L"\r\n");

        Print(L"SMU POST               : PASS\r\n");
        Print(L"Segments               : %u\r\n",
            AMD3DC_SEGMENT_COUNT);
        Print(L"Disabled segments      : %u\r\n",
            AMD3DC_DISABLED_SEGMENTS);

        Print(L"\r\n");

        Print(L"iGPU                   : GOP\r\n");
        Print(L"Resolution             : %u x %u\r\n",
            Gop->Mode->Info->HorizontalResolution,
            Gop->Mode->Info->VerticalResolution);

        Print(L"\r\n");
        Print(L"AMD 3DC EFI ready.\r\n");
        Print(L"\r\n");
    }

    return EFI_SUCCESS;
}