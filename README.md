# ESP32C61（ESP32-C61-DevKitC-1）
<img width="605" height="438" alt="image" src="https://github.com/user-attachments/assets/6d4d58af-a8fe-4e66-a807-e5cf46235f3b" />

# 开发板
- 当前固件在官方开发板ESP32-C61-DevKitC-1上测试成功
- ESP32-C61-DevKitC-1请使用USB口连接PC用于与EasyCon进行通讯，官方开发板标记在typec口内侧
- ESP32-C61-DevKitC-1请使用左侧标记为UART的Type-C口烧写固件

# 驱动
- 开发板默认USB转UART芯片通常为ch210x [乐鑫官方下载地址](https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads)
- 搞不懂的找卖你板子的商家

# 下载
[github releases](https://github.com/EasyConNS/EasyMCU_ESP32C61/releases)

# 已知问题
- 暂时只提供手柄功能，默认颜色与Pro2一致，按钮颜色为Pokemon ZA绿，其他功能未全量测试，不建议使用
- **不支持Amiibo**
- 暂未支持修改颜色
- 暂未支持修改序列号
- **暂时会提示手柄升级固件，请勿点击升级**，没改的原因是升级会导致协议变化，暂时未逆向升级版本的数据包

# 烧写
- 使用板子官方的下载工具 [flash_downlaod_tool](https://dl.espressif.com/public/flash_download_tool.zip),注意版本,旧版不支持ESP32C61
- 下载工具使用说明请参考 [Flash 下载工具用户指南](https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/tools/flash_download_tool.html)
- 搞不懂的先问AI，一旦界面上显示“SYNC”字样，不是你没选对口，就是没进入下载模式
- ESP32-C61-DevKitC-1 固件烧录图示，按图操作
<img width="221" height="213" alt="image" src="https://github.com/user-attachments/assets/ab3dca87-ae87-4e71-82fc-16f5882ef45c" />
<br />
<img width="636" height="676" alt="image" src="https://github.com/user-attachments/assets/e57e2344-3af9-4759-894d-69cb70928502" />
<br />
<img width="498" height="222" alt="image" src="https://github.com/user-attachments/assets/a98a980e-8881-4447-bd11-23374e0f1033" />
<br />
<img width="517" height="226" alt="image" src="https://github.com/user-attachments/assets/352b5791-61b5-4779-818a-5480b7a35f88" />

# 配对
进入NS2主机的更改手柄的握法/顺序界面，板子会自动配对，待看见手柄图标出现后，按NS2的A键退出即可
<img width="340" height="300" alt="image" src="https://github.com/user-attachments/assets/bbe1a05e-3a1d-4417-a967-1c39334c9b6b" />
<br />
后续板子接电会自动发起唤醒广播重连NS2，NS2如果在休眠模式会自动亮机（请勿插着板子过夜，不然NS2就要做仰卧起坐了）  
**取消配对功能暂未完全测试**，请使用EasyCon的ESP32取消配对功能尝试，如失败请手动在NS2中断开与手柄的连接，然后重新烧写
