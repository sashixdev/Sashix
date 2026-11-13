bits 16
org 0x8000

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x8000
    mov [boot_drv], dl

    mov byte [0x0500], 0

    mov ax, 0x0003
    int 0x10

    mov byte [os_count], 1

    mov dl, 0x81
    xor eax, eax
    call probe_sector

    mov dl, [boot_drv]
    mov eax, [0x7C00 + 462 + 8]
    test eax, eax
    jz .check_part3
    call probe_sector

.check_part3:
    mov dl, [boot_drv]
    mov eax, [0x7C00 + 478 + 8]
    test eax, eax
    jz .draw_menu
    call probe_sector

.draw_menu:
    mov ax, 0x0003
    int 0x10

    mov si, msg_header
    call print_str

    mov si, msg_opt1
    call print_str

    xor cx, cx
    mov cl, [os_count]
    dec cl
    jz .menu_footer

    mov bx, 0
.print_loop:
    mov al, ' '
    call print_char
    mov al, ' '
    call print_char
    mov al, '['
    call print_char
    mov al, bl
    add al, '2'
    call print_char
    mov al, ']'
    call print_char
    mov al, ' '
    call print_char

    mov di, bx
    add di, di
    mov si, [os_names + di]
    call print_str
    mov si, msg_crlf
    call print_str

    inc bx
    dec cl
    jnz .print_loop

.menu_footer:
    mov si, msg_prompt
    call print_str

    xor ah, ah
    int 0x1A
    mov bx, dx
    add bx, 182

.wait_key:
    mov ah, 0x01
    int 0x16
    jnz .key_pressed

    xor ah, ah
    int 0x1A
    cmp dx, bx
    jb .wait_key
    jmp .boot_sashix

.key_pressed:
    xor ah, ah
    int 0x16

    cmp al, 's'
    je .dev_menu
    cmp al, 'S'
    je .dev_menu

    cmp al, '1'
    je .boot_sashix

    sub al, '2'
    jb .wait_key
    xor ah, ah
    mov si, ax
    inc al
    cmp al, [os_count]
    jae .wait_key

    mov dl, [os_drives + si]
    add si, si
    add si, si
    mov eax, [os_lbas + si]
    call chainload_os
    jmp .draw_menu

.dev_menu:
    mov ax, 0x0003
    int 0x10

    mov si, msg_dev_menu
    call print_str

.dev_wait:
    xor ah, ah
    int 0x16

    cmp al, 27
    je .draw_menu

    cmp al, '1'
    je .set_fps

    cmp al, '2'
    je .set_debug

    jmp .dev_wait

.set_fps:
    mov byte [0x0500], 1
    jmp .boot_sashix

.set_debug:
    mov byte [0x0500], 2
    jmp .boot_sashix

.boot_sashix:
    mov si, msg_loading
    call print_str

.wait_kbd_1:
    in al, 0x64
    test al, 2
    jnz .wait_kbd_1
    mov al, 0xAD
    out 0x64, al

.wait_kbd_2:
    in al, 0x64
    test al, 2
    jnz .wait_kbd_2
    mov al, 0xD0
    out 0x64, al

.wait_kbd_3:
    in al, 0x64
    test al, 1
    jz .wait_kbd_3
    in al, 0x60
    push ax

.wait_kbd_4:
    in al, 0x64
    test al, 2
    jnz .wait_kbd_4
    mov al, 0xD1
    out 0x64, al

.wait_kbd_5:
    in al, 0x64
    test al, 2
    jnz .wait_kbd_5
    pop ax
    or al, 0x43
    out 0x60, al

.wait_kbd_6:
    in al, 0x64
    test al, 2
    jnz .wait_kbd_6
    mov al, 0xAE
    out 0x64, al

.wait_kbd_7:
    in al, 0x64
    test al, 2
    jnz .wait_kbd_7

    xor ax, ax
    mov ds, ax
    mov es, ax

    mov ax, 0x4F01
    mov cx, 0x011B
    mov di, 0x5000
    int 0x10

    mov ax, 0x4F02
    mov bx, 0x411B
    int 0x10

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:pm_code

probe_sector:
    push bx
    push cx
    push dx
    push si
    push di

    cmp byte [os_count], 5
    jae .probe_done

    mov [probe_drv], dl
    mov [probe_lba], eax

    test eax, eax
    jnz .read_lba

    mov ax, 0x0201
    mov cx, 0x0001
    xor dh, dh
    xor bx, bx
    mov es, bx
    mov bx, 0x7E00
    int 0x13
    jc .probe_done
    jmp .check_sig

.read_lba:
    mov [dap_probe + 8], eax
    mov ah, 0x42
    mov si, dap_probe
    int 0x13
    jc .probe_done

.check_sig:
    cmp word [0x7FFE], 0xAA55
    jne .probe_done

    mov si, str_bootmgr
    call find_in_buf
    jc .found_win

    mov si, str_ntldr
    call find_in_buf
    jc .found_win

    mov si, str_grub
    call find_in_buf
    jc .found_grub

    mov dx, name_other
    jmp .register_os

.found_win:
    mov dx, name_win
    jmp .register_os

.found_grub:
    mov si, str_ubuntu
    call find_in_buf
    jc .found_ubuntu

    mov si, str_arch
    call find_in_buf
    jc .found_arch

    mov dx, name_grub
    jmp .register_os

.found_ubuntu:
    mov dx, name_ubuntu
    jmp .register_os

.found_arch:
    mov dx, name_arch

.register_os:
    xor bx, bx
    mov bl, [os_count]
    dec bl

    mov al, [probe_drv]
    mov [os_drives + bx], al

    add bx, bx
    mov [os_names + bx], dx

    add bx, bx
    mov eax, [probe_lba]
    mov [os_lbas + bx], eax

    inc byte [os_count]

.probe_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

find_in_buf:
    push bx
    push cx
    push di
    mov bx, 0x7E00
.outer:
    cmp bx, 0x7FF0
    jae .not_found
    mov di, bx
    push si
.inner:
    lodsb
    or al, al
    jz .matched
    cmp al, [di]
    jne .next_pos
    inc di
    jmp .inner
.next_pos:
    pop si
    inc bx
    jmp .outer
.matched:
    pop si
    stc
    pop di
    pop cx
    pop bx
    ret
.not_found:
    clc
    pop di
    pop cx
    pop bx
    ret

chainload_os:
    mov [chain_drv], dl
    mov [chain_lba], eax

    mov si, msg_chain
    call print_str

    mov eax, [chain_lba]
    test eax, eax
    jnz .chain_lba_read

    mov ax, 0x0201
    mov cx, 0x0001
    xor dh, dh
    mov dl, [chain_drv]
    xor bx, bx
    mov es, bx
    mov bx, 0x7C00
    int 0x13
    jc .chain_fail
    jmp .chain_jump

.chain_lba_read:
    mov [dap_chain + 8], eax
    mov ah, 0x42
    mov dl, [chain_drv]
    mov si, dap_chain
    int 0x13
    jc .chain_fail

.chain_jump:
    cmp word [0x7DFE], 0xAA55
    jne .chain_fail
    mov dl, [chain_drv]
    jmp 0x0000:0x7C00

.chain_fail:
    mov si, msg_chain_err
    call print_str
    ret

print_char:
    mov ah, 0x0E
    int 0x10
    ret

print_str:
    mov ah, 0x0E
.p_loop:
    lodsb
    or al, al
    jz .p_done
    int 0x10
    jmp .p_loop
.p_done:
    ret

bits 32
pm_code:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    cld
    jmp 0x08:0x9000

align 4
dap_probe: db 0x10, 0x00, 1, 0, 0x00, 0x7E, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0, 0
dap_chain: db 0x10, 0x00, 1, 0, 0x00, 0x7C, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0, 0

boot_drv:  db 0
probe_drv: db 0
probe_lba: dd 0
chain_drv: db 0
chain_lba: dd 0
os_count:  db 1

os_names:  times 4 dw 0
os_drives: times 4 db 0
os_lbas:   times 4 dd 0

str_bootmgr: db 'BOOTMGR', 0
str_ntldr:   db 'NTLDR', 0
str_grub:    db 'GRUB', 0
str_ubuntu:  db 'Ubuntu', 0
str_arch:    db 'Arch', 0

name_win:    db 'Windows Boot Manager', 0
name_ubuntu: db 'Ubuntu (GRUB)', 0
name_arch:   db 'Arch Linux (GRUB)', 0
name_grub:   db 'Linux (GRUB Bootloader)', 0
name_other:  db 'External OS (Chainload)', 0

msg_header:
    db '==============================================', 13, 10
    db '            SASHIX OS BOOT MANAGER            ', 13, 10
    db '==============================================', 13, 10, 13, 10, 0

msg_opt1:      db '  [1] Sashix OS (Default)', 13, 10, 0
msg_crlf:      db 13, 10, 0
msg_prompt:    db 13, 10, 'Press number to boot or [S] for Developer Mode (10s): ', 0

msg_dev_menu:
    db '==============================================', 13, 10
    db '           SASHIX DEVELOPER OPTIONS           ', 13, 10
    db '==============================================', 13, 10, 13, 10
    db '  [1] Sashix (FPS Monitor)', 13, 10
    db '  [2] Sashix (Debug Action Log)', 13, 10, 13, 10
    db 'Select mode [1-2] or [ESC] to return: ', 0

msg_chain:     db 13, 10, '[*] Chainloading selected OS...', 13, 10, 0
msg_chain_err: db '[!] Failed to load selected OS.', 13, 10, 0
msg_loading:   db 13, 10, '[*] Starting sasKernel...', 13, 10, 0

align 4
gdt_start:
    dd 0x00000000
    dd 0x00000000

    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 4096-($-$$) db 0