libusbserial
============

library for accessing USB to Serial Adapters using libusb

Demo：

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libusbserial.h>
#include <internal.h>


//接收数据测试还是有问题
void read_cb(
       void * data, unsigned int bytes_count,
      void * user_data)
       {
           printf("received size %d\n",bytes_count);
           printf("%c",(char)(*(char*)data));
          
       }


int main()
{
    char  data[]="hello world!\n";
    int res=-1;
    struct usbserial_port out;
    out.read_cb=read_cb;
    struct usbserial_port* out_port;
    out_port=&out;
    struct usbserial_line_config line_config;
    line_config.baud=115200;
    line_config.data_bits=USBSERIAL_DATABITS_8;
    line_config.stop_bits=USBSERIAL_STOPBITS_1;
    line_config.parity=USBSERIAL_PARITY_NONE;
    libusb_device_handle* usb_device_handle=NULL;
    libusb_context *ctx = NULL;
    res=usbserial_init();
    if(res!=0)
    {
      printf("usb serial init error !\n");
      return -1;
    }
    else
    printf("usb serial init success!\n");
    res = libusb_init(&ctx);
    res=usbserial_is_device_supported(0x067b,0x2303,0x02,0x02);
    printf("res%d\n",res);
    const char * ptr=usbserial_get_device_short_name(0x067b,0x2303,0x02,0x02);
    printf("the serial name: %s\n",ptr);
    res=-1;
    res=usbserial_get_ports_count(0x067b,0x2303,0x02,0x02);
    printf("serial port avail: %d \n",res);
    usb_device_handle=libusb_open_device_with_vid_pid(ctx, 0x067b, 0x2303);
    if(usb_device_handle==NULL)
    {
      printf("device handle err!\n");
      return -2;
    }
    printf("device handle ok!\n");
    
    res=usbserial_port_init(
        &out_port,
        usb_device_handle,
        0,
        read_cb,
        NULL,
        NULL);
        printf("port init %d \n",res);
    res=usbserial_port_set_line_config(
                                       out_port,
                                       &line_config);
    printf("line config success!%d\n",res);
    usbserial_start_reader(out_port);
while(1)
{
    //printf("data size :%d\n",sizeof(data));
    res=usbserial_write(out_port,data,sizeof(data));
   // printf("send data success!\n");
    sleep(1);
}
    
      if (usb_device_handle)
      {
		      libusb_close (usb_device_handle);
	     }
   
      libusb_exit(ctx);
    return 0;

}

