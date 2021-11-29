org 0x10000                        ; Start from 1 MB

jmp Label_Start

RootDirSectors          equ 14      ; (BPB_RootEntCnt * 32 + BPB_BytesPerSec - 1) / BPB_BytesPerSec
SectorNumOfRootDirStart equ 19      ; BPB_RsvdSecCnt + BPB_NumFATs * BPB_FATSz16
SectorNumOfFAT1Start    equ 1       ; FAT1
SectorBalance           equ 17      ; the first cluster of data Section is 2

BS_OEMName          db 'MineBoot'   ; the name of the manufacturee
BPB_BytesPerSec     dw 512          ; Bytes Per Sector
BPB_SecPerClus      db 1            ; Sectors Per Cluster
BPB_RsvdSecCnt      dw 1            ; Reserved Clusters
BPB_NumFATs         db 2            ; Number of FAT tables (Suggest to be 2, one for primary and the other for backup)
BPB_RootEntCnt      dw 224          ; Directory entries that can be contained in the root directory ( BPB_RootEntCnt * 32 / BPB_BytesPerSec = an arbitrary even number)
BPB_TotSec16        dw 2880         ; Total Sector count
BPB_Media           db 0xf0         ; Media type (Must be the same with the low byte of FAT[0])
BPB_FATSz16         dw 9            ; The size of FAT table
BPB_SecPerTrk       dw 18           ; Sectors Per Track
BPB_NumHeads        dw 2            ; Number of track header
BPB_hiddSec         dd 0            ; Number of hidden Sectors
BPB_TotSec32        dd 0            ; If BPB_TotSec16 is zero, this value must be set
BS_DrvNum           db 0            ; the driver of int 13h
BS_Reserved1        db 0            ; not used
BS_BootSig          db 29h          ; The sign of boot extend
BS_VolID            dd 0            ; Volumn Number
BS_VolLab           db 'boot loader'; Volumn name
BS_FileSysType      db 'FAT12   '   ; Type of file system


BaseOfKernelFile        equ     0x00
OffsetOfKernelFile      equ     0x100000

BaseTmpOfKernelAddr     equ     0x00
OffsetTmpOfKernelFile   equ     0x7e00

MemoryStructBufferAddr  equ     0x7e00

[SECTION gdt]

LABEL_GDT:              dd      0, 0
LABEL_DESC_CODE32:      dd      0x0000ffff,0x00cf9a00
LABEL_DESC_DATA32:      dd      0x0000ffff,0x00cf9200

GdtLen                  equ     $ - LABEL_GDT
GdtPtr                  dw      GdtLen - 1
                        dd      LABEL_GDT

SelectorCode32          equ     LABEL_DESC_CODE32 - LABEL_GDT
SelectorData32          equ     LABEL_DESC_DATA32 - LABEL_GDT

[SECTION gdt64]

LABEL_GDT64:            dq      0x0000000000000000
LABEL_DESC_CODE64:      dq      0x0020980000000000
LABEL_DESC_DATA64:      dq      0x0000920000000000

GdtLen64                equ     $ - LABEL_GDT64
GdtPtr64                dw      GdtLen64 - 1
                        dd      LABEL_GDT64

SelectorCode64          equ     LABEL_DESC_CODE64 - LABEL_GDT64
SelectorData64          equ     LABEL_DESC_DATA64 - LABEL_GDT64

[SECTION .16]
[BITS 16]    ; Generate the code which run on mode 16-bits process

Label_Start:

mov         ax,     cs
mov         ds,     ax 
mov         es,     ax
xor         ax,     ax 
mov         ss,     ax 
mov         sp,     0x7c00

; =========== clear screen
    mov         ax,     0600h
    mov         bx,     0700h
    mov         cx,     0
    mov         dx,     0184fh
    int         10h

; =========== open address A20
; be able to find address above 1MB

    push        ax
    in          al,     92h
    or          al,     00000010b
    out         92h,    al
    pop         ax
; Fast Gate A20 : Port 92h
    cli

    db          0x66    ; use 32-bit data command
    lgdt        [GdtPtr]

    mov         eax,    cr0
    or          eax,    1 
    mov         cr0,    eax ; turn to the protect mode

    mov         ax,     SelectorData32 ; open the 32-bit-addr-search on fs
    mov         fs,     ax 
    mov         eax,    cr0 
    and         al,     11111110b ; turn to the real mode
    mov         cr0,    eax 

    sti

; ========== reset floppy

    xor         ah,     ah 
    xor         dl,     dl 
    int         13h
    
; ========== search kernel.bin

Label_Start_Search:
    mov         word    [SectorNo],     SectorNumOfRootDirStart

Label_Search_In_Root_Dir_Begin:
    cmp         word    [RootDirSizeForLoop],   0
    jz          Label_No_KernelBin
    dec         word    [RootDirSizeForLoop]
    mov         ax,     00h
    mov         es,     ax
    mov         bx,     8000h
    mov         ax,     [SectorNo]
    mov         cl,     1
    call        Func_ReadOneSector
    mov         si,     KernelFileName
    mov         di,     8000h
    cld 
    mov         dx,     10h

Label_Search_For_KernelBin:
    cmp         dx,     0
    jz          Label_Goto_Next_Sector_In_Root_Dir
    dec         dx
    mov         cx,     11

Label_Cmp_FileName:
    cmp         cx,     0
    jz          Label_FileName_Found 
    dec         cx 
    lodsb 
    cmp         al,     byte    [es:di]
    jz          Label_Go_On 
    jmp         Label_Different

Label_Go_On:
    inc         di
    jmp         Label_Cmp_FileName

Label_Different:
    and         di,     0ffe0h
    add         di,     20h
    mov         si,     KernelFileName
    jmp         Label_Search_For_KernelBin

Label_Goto_Next_Sector_In_Root_Dir:
    add         word    [SectorNo],     1
    jmp         Label_Search_In_Root_Dir_Begin

; ========= Display on screen : ERROR: NO LOADER FOUND

Label_No_KernelBin:
    mov         ax,     1301h
    mov         bx,     008ch
    mov         dx,     0100h
    mov         cx,     22
    push        ax
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     NoKernelMessage
    int         10h
    jmp         $


; ======= found kernel.bin name in root director struct

Label_FileName_Found:

    mov         ax,     RootDirSectors
    and         di,     0ffe0h
    add         di,     01ah
    mov         cx,     word    [es:di]
    push        cx
    add         cx,     ax
    add         cx,     SectorBalance
    mov         ax,     BaseTmpOfKernelAddr
    mov         es,     eax
    mov         bx,     OffsetTmpOfKernelFile
    mov         ax,     cx 

Label_Go_On_Loading_File:
    push        ax
    push        bx 
    mov         ah,     0eh 
    mov         al,     '.'
    mov         bl,     0fh 
    int         10h 
    pop         bx 
    pop         ax 
    
    mov         cl,     1
    call        Func_ReadOneSector
    pop         ax

    push        cx
    push        eax 
    push        fs 
    push        edi 
    push        ds 
    push        esi 

    mov         cx,     200h
    mov         ax,     BaseOfKernelFile
    mov         fs,     ax 
    mov         edi,    dword   [OffsetOfKernelFileCount]

    mov         ax,     BaseTmpOfKernelAddr
    mov         ds,     ax 
    mov         esi,    OffsetTmpOfKernelFile

Label_Mov_Kernel:
    mov         al,     byte    [ds:esi]
    mov         byte    [fs:edi],   al 

    inc         esi 
    inc         edi 

    loop        Label_Mov_Kernel

    mov         eax,    0x1000
    mov         ds,     eax

    mov         dword   [OffsetOfKernelFileCount],  edi
    
    pop         esi 
    pop         ds 
    pop         edi 
    pop         fs 
    pop         eax 
    pop         cx 

    call        Func_GetFATEntry
    cmp         ax,     0fffh 
    jz          Label_File_Loaded 
    push        ax 
    mov         dx,     RootDirSectors
    add         ax,     dx 
    add         ax,     SectorBalance
    jmp         Label_Go_On_Loading_File

Label_File_Loaded:
    mov         ax,     0b800h
    mov         gs,     ax 
    mov         ah,     0fh               ; 0000 black ground, 1111 white word
    mov         al,     'G'
    mov         [gs:((80*0 + 39) *2)], ax ; line 0, col 39

; ======  Close the floppy
KillMotor:

    push        dx
    mov         dx,     03f2h
    mov         al,     0 
    out         dx,     al 
    pop         dx 

; ====== Get memory address size type
    mov         ax,     1301h
    mov         bx,     000fh
    mov         dx,     0400h
    mov         cx,     24
    push        ax 
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     StartGetMemStructMessage
    int         10h     

    xor         ebx,    ebx 
    xor         ax,     ax 
    mov         es,     ax 
    mov         di,     MemoryStructBufferAddr

Label_Get_Mem_Struct:

    mov         eax,    0x0e820
    mov         ecx,    20
    mov         edx,    0x534d4150
    int         15h
    jc          Label_Get_Mem_Fail
    add         di,     20
    cmp         ebx,    0
    jne         Label_Get_Mem_Struct
    jmp         Label_Get_Mem_OK

Label_Get_Mem_Fail:

    mov         ax,     1301h
    mov         bx,     008ch
    mov         dx,     0500h
    mov         cx,     23
    push        ax 
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     GetMemStructErrMessage
    int         10h 
    jmp         $

Label_Get_Mem_OK:

    mov         ax,     1301h
    mov         bx,     000fh
    mov         dx,     0600h
    mov         cx,     29
    push        ax 
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     GetMemStructOKMessage
    int         10h   

; ====== get SVGA info

    mov         ax,     1301h
    mov         bx,     000fh
    mov         dx,     0800h ; row 8
    mov         cx,     23 
    push        ax 
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     StartGetSvgaVbeInfoMessage
    int         10h 

    xor         ax,     ax 
    mov         es,     ax 
    mov         di,     0x8000
    mov         ax,     4f00h 

    int         10h 

    cmp         ax,     004fh 

    jz          .KO 

;======   Fail

    mov         ax,     1301h
    mov         bx,     008ch 
    mov         dx,     0900h ;row 9
    mov         cx,     23 
    push        ax
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     GetSvgaVbeInfoErrMessage
    int         10h 
    jmp         $

;======  Success
.KO:

    mov         ax,     1301h
    mov         bx,     000fh 
    mov         dx,     0a00h ; row 10
    mov         cx,     29 
    push        ax 
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     GetSvgaVbeInfoOKMessage
    int         10h 

; ====== Get Svga mode info

    mov         ax,     1301h
    mov         bx,     000fh 
    mov         dx,     0c00h ; row 12
    mov         cx,     24 
    push        ax 
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     StartGetSvgaModeInfoMessage
    int         10h 

    xor         ax,     ax 
    mov         es,     ax 
    mov         si,     0x800e
    mov         esi,    dword   [es:si]
    mov         edi,    0x8200

Label_SVGA_Mode_Info_Get:

    mov         cx,     word    [es:esi]

; ======  display Svga mode info
    
    push        ax 

    mov         ax,     00h 
    mov         al,     ch
    call        Func_Disp_AL

    mov         ax,     00h
    mov         al,     cl 
    call        Func_Disp_AL

    pop         ax 

; ======
    
    cmp         cx,     0ffffh
    jz          Label_SVGA_Mode_Info_Finish

    mov         ax,     4f01h 
    int         10h 

    cmp         ax,     004fh 

    jnz         Label_SVGA_Mode_Info_Fail

    add         esi,    2 
    add         edi,    0x100 

    jmp         Label_SVGA_Mode_Info_Get

Label_SVGA_Mode_Info_Fail:

    mov         ax,     1301h
    mov         bx,     000fh 
    mov         dx,     0d00h ; row 13
    mov         cx,     24 
    push        ax 
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     GetSvgaModeInfoErrMessage
    int         10h 

Label_Set_SVGA_Fail:

    jmp         $

Label_SVGA_Mode_Info_Finish:
    mov         ax,     1301h
    mov         bx,     000fh 
    mov         dx,     0e00h ; row 14
    mov         cx,     30
    push        ax 
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     GetSvgaModeInfoOKMessage
    int         10h 

; ====== set the svga mode (vesa vbe)

    mov         ax,     4f02h
    mov         bx,     4180h
    int         10h 

    cmp         ax,     004fh
    jnz         Label_Set_SVGA_Fail

; ======  init idt gdt goto protect mode

    cli 
    db          0x66
    lgdt        [GdtPtr]

    mov         eax,    cr0 
    or          eax,    1
    mov         cr0,    eax 

    jmp         dword   SelectorCode32:Go_To_Tmp_Protect

[SECTION .s32] 
[BITS 32]

Go_To_Tmp_Protect:

;======  go to tmp long mode

    mov         ax,     0x10 
    mov         ds,     ax 
    mov         es,     ax 
    mov         fs,     ax 
    mov         ss,     ax 
    mov         esp,    7e00h 

    call        support_long_mode 

    test        eax,    eax 

    jz          Label_No_Long_Mode_Support

; =====  init temporary page table 0x90000

    mov         dword   [0x90000],  0x91007
    mov         dword   [0x90800],  0x91007
    mov         dword   [0x91000],  0x92007
    mov         dword   [0x92000],  0x000083
    mov         dword   [0x92008],  0x200083
    mov         dword   [0x92010],  0x400083
    mov         dword   [0x92018],  0x600083
    mov         dword   [0x92020],  0x800083
    mov         dword   [0x92028],  0xa00083

; ======  load gdtr

    db          0x66
    lgdt        [GdtPtr64]
    mov         ax,     0x10
    mov         ds,     ax 
    mov         es,     ax 
    mov         fs,     ax 
    mov         ss,     ax 
    mov         esp,    7e00h 

; ===== open PAE
    mov         eax,    cr4 
    bts         eax,    5 
    mov         cr4,    eax 

; ===== load cr3

    mov         eax,    0x90000
    mov         cr3,    eax 

; ===== enable long-mode

    mov         ecx,    0c0000080h ;IA32_EFER
    rdmsr 

    bts         eax,    8 
    wrmsr 

; ====== open PE and paging

    mov         eax,    cr0 
    bts         eax,    0 
    bts         eax,    31
    mov         cr0,    eax 

    jmp         SelectorCode64:OffsetOfKernelFile

; ====== test support long mode or not

support_long_mode:

    mov         eax,    0x80000000
    cpuid 
    cmp         eax,    0x80000001
    setnb       al 
    jb          support_long_mode_done 
    mov         eax,    0x80000001
    cpuid       
    bt          edx,     29
    setc        al 

support_long_mode_done:
    movzx       eax,    al 
    ret 

; ===== no support 

Label_No_Long_Mode_Support:
    jmp $


[SECTION .s16lib]
[BITS 16]

Func_ReadOneSector:
    push        bp
    mov         bp,     sp 
    sub         esp,    2
    mov         byte    [bp-2], cl 
    push        bx 
    mov         bl,     [BPB_SecPerTrk]
    div         bl 
    inc         ah 
    mov         cl,     ah 
    mov         dh,     al
    shr         al,     1
    mov         ch,     al
    and         dh,     1 
    pop         bx
    mov         dl,     [BS_DrvNum]

Label_Go_On_Reading:
    mov         ah,     2
    mov         al,     byte    [bp-2]
    int         13h
    jc          Label_Go_On_Reading
    add         esp,    2
    pop         bp
    ret 

; ========= Get FAT Entry

Func_GetFATEntry:
    push        es
    push        bx 
    push        ax 
    mov         ax,     00
    mov         es,     ax 
    pop         ax
    mov         byte    [Odd],  0
    mov         bx,     3
    mul         bx
    mov         bx,     2
    div         bx 
    cmp         dx,     0
    jz          Label_Even
    mov         byte    [Odd],  1

Label_Even:
    xor         dx,     dx
    mov         bx,     [BPB_BytesPerSec]
    div         bx
    push        dx 
    mov         bx,     8000h
    add         ax,     SectorNumOfFAT1Start
    mov         cl,     2
    call        Func_ReadOneSector

    pop         dx 
    add         bx,     dx
    mov         ax,     [es:bx]
    cmp         byte    [Odd],  1
    jnz         Label_Even_2
    shr         ax,     4

Label_Even_2:
    and         ax,     0fffh
    pop         bx 
    pop         es 
    ret 

; ======= display num in al

Func_Disp_AL:

    push        ecx 
    push        edx 
    push        edi 

    mov         edi,    [DisplayPosition]
    mov         ah,     0fh 
    mov         dl,     al 
    shr         al,     4
    mov         ecx,    2
.begin:

    and         al,     0fh 
    cmp         al,     9
    ja          .1
    add         al,     '0'
    jmp         .2 

.1:

    sub         al,     0ah 
    add         al,     'A'

.2: 

    mov         [gs:edi],   ax
    add         edi,    2

    mov         al,     dl 
    loop        .begin

    mov         [DisplayPosition],  edi 
    
    pop         edi 
    pop         edx 
    pop         ecx 
    ret 

; ====== tmp IDT

IDT:                        times   0x50    dq  0
IDT_END:
IDT_POINTER:                dw      IDT_END - IDT - 1
                            dd      IDT

; ====== tmp var

RootDirSizeForLoop          dw      RootDirSectors
SectorNo                    dw      0
Odd                         db      0
OffsetOfKernelFileCount     dd      OffsetOfKernelFile

DisplayPosition             dd      0

; ====== Strings
NoKernelMessage:            db  "ERROR: NO KERNEL FOUND"
KernelFileName:             db  "KERNEL  BIN", 0
StartGetMemStructMessage:   db  "START GET MEMORY STRUCT"
GetMemStructErrMessage:     db  "GET MEMORY STRUCT ERROR"
GetMemStructOKMessage:      db  "GET MEMORY STRUCT SUCCESSFUL!"

StartGetSvgaVbeInfoMessage: db  "START GET SVGA VBE INFO"
GetSvgaVbeInfoErrMessage:   db  "GET SVGA VBE INFO ERROR"
GetSvgaVbeInfoOKMessage:    db  "GET SVGA VBE INFO SUCCESSFUL!"

StartGetSvgaModeInfoMessage:db "START GET SVGA MODE INFO"
GetSvgaModeInfoErrMessage:  db "GET SVGA MODE INFO ERROR"
GetSvgaModeInfoOKMessage:   db "GET SVGA MODE INFO SUCCESSFUL!"