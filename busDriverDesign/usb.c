#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

static struct input_dev *myusb_mouse_dev;              //input_dev 

static char *myusb_mouse_buf;                         //虚拟地址缓存区
static dma_addr_t myusb_mouse_phyc;                     //DMA缓存区;

static __le16 myusb_mouse_size;                        //数据包长度

static struct urb  *myusb_mouse_urb;                     //urb


static void myusb_mouse_irq(struct urb *urb)               //鼠标中断函数
{
   static char buf1=0;
   //for(i=0;i<myusb_mouse_size;i++)
  // printk("%02x  ",myusb_mouse_buf[i]);
 //  printk("\n");


    /*bit 1-左右中键  0X01:左键   0X02:右键     0x04:中键   */
     if((buf1&(0X01))    !=      (myusb_mouse_buf[1]&(0X01)))
        {         
                input_report_key(myusb_mouse_dev, KEY_L, buf1&(0X01)? 1:0);
                input_sync(myusb_mouse_dev);  
        }
     if((buf1&(0X02))    !=    (myusb_mouse_buf[1]&(0X02)))
        {
                input_report_key(myusb_mouse_dev, KEY_S, buf1&(0X02)? 1:0);
                input_sync(myusb_mouse_dev);  
        }
     if((buf1&(0X04))    !=    (myusb_mouse_buf[1]&(0X04))  )
          {
                input_report_key(myusb_mouse_dev, KEY_ENTER, buf1&(0X04)? 1:0);
                input_sync(myusb_mouse_dev);  
          }

     buf1=myusb_mouse_buf[1];                                   //更新数据

     usb_submit_urb(myusb_mouse_urb, GFP_KERNEL);
}


static int myusb_mouse_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
       struct usb_device *dev = interface_to_usbdev(intf);             //设备
       struct usb_endpoint_descriptor *endpoint;                            
       struct usb_host_interface *interface;                         //当前接口
       int pipe;                                                   //端点管道

       interface=intf->cur_altsetting;                                                                   
       endpoint = &interface->endpoint[0].desc;                                    //当前接口下的端点描述符
   

       printk("VID=%x,PID=%x\n",dev->descriptor.idVendor,dev->descriptor.idProduct); //打印VID,PID
 

 /*   1)分配一个input_dev结构体  */
       myusb_mouse_dev=input_allocate_device();

 
 /*   2)设置input_dev支持L、S,回车、3个按键事件*/
       set_bit(EV_KEY, myusb_mouse_dev->evbit);
       set_bit(EV_REP, myusb_mouse_dev->evbit);        //支持重复按功能

       set_bit(KEY_L, myusb_mouse_dev->keybit);       
       set_bit(KEY_S, myusb_mouse_dev->keybit);
       set_bit(KEY_ENTER, myusb_mouse_dev->keybit);    

 /*   3)注册input_dev结构体*/
       input_register_device(myusb_mouse_dev);

 
 /*   4)设置USB数据传输 */
 /*->4.1)通过usb_rcvintpipe()创建一个端点管道*/
       pipe=usb_rcvintpipe(dev,endpoint->bEndpointAddress); 

  /*->4.2)通过usb_buffer_alloc()申请USB缓冲区*/
       myusb_mouse_size=endpoint->wMaxPacketSize;
       myusb_mouse_buf=usb_buffer_alloc(dev,myusb_mouse_size,GFP_ATOMIC,&myusb_mouse_phyc);

  /*->4.3)通过usb_alloc_urb()和usb_fill_int_urb()申请并初始化urb结构体 */
       myusb_mouse_urb=usb_alloc_urb(0,GFP_KERNEL);

       usb_fill_int_urb (myusb_mouse_urb,                       //urb结构体
                                 dev,                           //usb设备
                                 pipe,                          //端点管道
                                 myusb_mouse_buf,               //缓存区地址
                                 myusb_mouse_size,              //数据长度
                                 myusb_mouse_irq,               //中断函数
                                 0,
                                 endpoint->bInterval);          //中断间隔时间


  /*->4.4) 因为我们2440支持DMA,所以要告诉urb结构体,使用DMA缓冲区地址*/
        myusb_mouse_urb->transfer_dma   =myusb_mouse_phyc;             //设置DMA地址
        myusb_mouse_urb->transfer_flags   =URB_NO_TRANSFER_DMA_MAP;     //设置使用DMA地址

  /*->4.5)使用usb_submit_urb()提交urb*/
        usb_submit_urb(myusb_mouse_urb, GFP_KERNEL);
       return 0;
}

static void myusb_mouse_disconnect(struct usb_interface *intf)
{
    struct usb_device *dev = interface_to_usbdev(intf);                  //设备

    usb_kill_urb(myusb_mouse_urb);
    usb_free_urb(myusb_mouse_urb);
    usb_buffer_free(dev, myusb_mouse_size, myusb_mouse_buf,myusb_mouse_phyc);

    input_unregister_device(myusb_mouse_dev);         //注销内核中的input_dev
    input_free_device(myusb_mouse_dev);             //释放input_dev
} 

static struct usb_device_id myusb_mouse_id_table [] = {
       { USB_INTERFACE_INFO(
              USB_INTERFACE_CLASS_HID,                 //接口类:hid类
              USB_INTERFACE_SUBCLASS_BOOT,             //子类:启动设备类
              USB_INTERFACE_PROTOCOL_MOUSE) },      //USB协议:鼠标协议
};


static struct usb_driver myusb_mouse_drv = {
       .name            = "myusb_mouse",
       .probe           = myusb_mouse_probe,                           
       .disconnect     = myusb_mouse_disconnect,
       .id_table  = myusb_mouse_id_table,
};
 
/*入口函数*/
static int myusb_mouse_init(void)
{
       usb_register(&myusb_mouse_drv);
       return 0;
}

/*出口函数*/
static void myusb_mouse_exit(void)
{
       usb_deregister(&myusb_mouse_drv);
}
 
module_init(myusb_mouse_init);
module_exit(myusb_mouse_exit);
MODULE_LICENSE("GPL");
