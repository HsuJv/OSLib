org 0x7c00

BaseOfStack     equ 0x7c00
BaseOfLoader    equ 0x1000
OffsetOfLoader  equ 0x00            ; the start of Loader is located on BaseOfLoader << 4 + OffsetOfLoader

RootDirSectors         equ 14      ; (BPB_RootEntCnt * 32 + BPB_BytesPerSec - 1) / BPB_BytesPerSec
SectorNumOfRootDirStart equ 19      ; BPB_RsvdSecCnt + BPB_NumFATs * BPB_FATSz16
SectorNumOfFAT1Start    equ 1       ; FAT1
SectorBalance           equ 17      ; the first cluster of data Section is 2

jmp short Label_Start
nop
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

; =========  read one sector from floppy
; int 13h , ah = 2
; read the sector of a disk
; al = the number of sectors
; ch = the low eight bit of the track id
; cl = the sector id (bit 0 ~ 5), the high two bit of the track id (only work for hard disk)
; dh = the track header id
; dl = the driver id (bit 7 must be set for hard disk)
; es:bx = data buffer

; Func_ReadOneSector:
; ax = the id of start sector
; cl = the number of sectors
; es:bx = data buffer
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



Label_Start:
mov         ax,     cs
mov         ds,     ax 
mov         es,     ax
mov         ss,     ax 
mov         sp,     0x7c00

; ========= search loader.bin
    mov         word    [SectorNo],     SectorNumOfRootDirStart

Label_Search_In_Root_Dir_Begin:
    cmp         word    [RootDirSizeForLoop],   0
    jz          Label_No_LoaderBin
    dec         word    [RootDirSizeForLoop]
    mov         ax,     00h
    mov         es,     ax
    mov         bx,     8000h
    mov         ax,     [SectorNo]
    mov         cl,     1
    call        Func_ReadOneSector
    mov         si,     LoaderFileName
    mov         di,     8000h
    cld 
    mov         dx,     10h

Label_Search_For_LoaderBin:
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
    mov         si,     LoaderFileName
    jmp         Label_Search_For_LoaderBin

Label_Goto_Next_Sector_In_Root_Dir:
    add         word    [SectorNo],     1
    jmp         Label_Search_In_Root_Dir_Begin

; ========= Display on screen : ERROR: NO LOADER FOUND

Label_No_LoaderBin:
    mov         ax,     1301h
    mov         bx,     008ch
    mov         dx,     0100h
    mov         cx,     22
    push        ax
    mov         ax,     ds 
    mov         es,     ax 
    pop         ax 
    mov         bp,     NoLoaderMessage
    int         10h
    jmp         $

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

; ======= found loader.bin name in root director struct

Label_FileName_Found:
    mov         ax,     RootDirSectors
    and         di,     0ffe0h
    add         di,     01ah
    mov         cx,     word    [es:di]
    push        cx
    add         cx,     ax
    add         cx,     SectorBalance
    mov         ax,     BaseOfLoader
    mov         es,     ax
    mov         bx,     OffsetOfLoader
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
    call        Func_GetFATEntry
    cmp         ax,     0fffh 
    jz          Label_File_Loaded 
    push        ax 
    mov         dx,     RootDirSectors
    add         ax,     dx 
    add         ax,     SectorBalance
    add         bx,     [BPB_BytesPerSec]
    jmp         Label_Go_On_Loading_File

Label_File_Loaded:
    jmp         BaseOfLoader:OffsetOfLoader

; ====== tmp variable
RootDirSizeForLoop  dw  RootDirSectors
SectorNo            dw  0
Odd                 db  0

; ====== Strings
StartBootMessage:   db  "Start Boot"
NoLoaderMessage:    db  "ERROR: NO LOADER FOUND"
LoaderFileName:     db  "LOADER  BIN", 0

; =========== fill zero till the sector end
    times 510 - ($ - $$) db 0
    dw 0xaa55
