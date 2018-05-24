ESC_CONT = $(SUBMODULE)/ESC_control

ESCCONTROLSRC = $(ESC_CONT)/src/esc_comm.c \
                $(ESC_CONT)/vesc_files/bldc_interface.c \
                $(ESC_CONT)/vesc_files/bldc_interface_uart.c \
                $(ESC_CONT)/vesc_files/buffer.c \
                $(ESC_CONT)/vesc_files/comm_uart.c \
                $(ESC_CONT)/vesc_files/crc.c \
                $(ESC_CONT)/vesc_files/packet.c \
                $(ESC_CONT)/vesc_files/esc_led.c

ESCCONTROLINC = $(ESC_CONT) \
                $(ESC_CONT)/include \
                $(ESC_CONT)/vesc_files