bits 16
org 0x7C00

start:
    jmp short main
    nop
    times 60 db 0

main:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .use_chs
    cmp bx, 0xAA55
    jne .use_chs

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap1
    int 0x13
    jc .use_chs

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap2
    int 0x13
    jc .use_chs

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap3
    int 0x13
    jc .use_chs

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap4
    int 0x13
    jnc .boot_ok

.use_chs:
    mov ah, 0x08
    mov dl, [boot_drive]
    xor di, di
    mov es, di
    int 0x13
    jc disk_error

    and cl, 0x3F
    mov [spt], cl
    inc dh
    mov [heads], dh

    mov word [target_seg], 0x0800
    mov word [cur_lba], 1
    mov word [sectors_left], 255

.chs_loop:
    mov ax, [cur_lba]
    xor dx, dx
    xor ch, ch
    mov cl, [spt]
    div cx
    inc dl
    mov cl, dl

    xor dx, dx
    xor bh, bh
    mov bl, [heads]
    div bx
    mov dh, dl
    mov ch, al
    shl ah, 6
    or cl, ah

    mov ax, 0x0201
    mov dl, [boot_drive]
    mov es, [target_seg]
    xor bx, bx
    int 0x13
    jc disk_error

    add word [target_seg], 0x20
    inc word [cur_lba]
    dec word [sectors_left]
    jnz .chs_loop

.boot_ok:
    mov dl, [boot_drive]
    jmp 0x0000:0x8000

disk_error:
    mov si, msg_error
    mov ah, 0x0E
.err_loop:
    lodsb
    or al, al
    jz .halt
    int 0x10
    jmp .err_loop
.halt:
    cli
    hlt

align 4
dap1: db 0x10, 0x00, 64, 0, 0x00, 0x80, 0x00, 0x00, 1, 0, 0, 0, 0, 0, 0, 0
dap2: db 0x10, 0x00, 64, 0, 0x00, 0x00, 0x00, 0x10, 65, 0, 0, 0, 0, 0, 0, 0
dap3: db 0x10, 0x00, 64, 0, 0x00, 0x00, 0x00, 0x18, 129, 0, 0, 0, 0, 0, 0, 0
dap4: db 0x10, 0x00, 64, 0, 0x00, 0x00, 0x00, 0x20, 193, 0, 0, 0, 0, 0, 0, 0

boot_drive:   db 0
spt:          db 18
heads:        db 2
cur_lba:      dw 1
sectors_left: dw 255
target_seg:   dw 0x0800
msg_error:    db 'DISK ERR', 0

times 490-($-$$) db 0

install_sig:  db 0, 0, 0, 0
install_user: times 16 db 0

dw 0xAA55