

# uart protocol

识别到色块时，将会把色块的四个顶点信息通过串口协议上报。

例如上报内容为：AA CA AC BB 14 00 00 00 E1 08 EE 00 37 00 15 01 F7 FF 4E 01 19 00 27 01 5A 00 A7 20，其中`08`是本次消息的命令码0x08, `EE 00 37 00 15 01 F7 FF 4E 01 19 00 27 01 5A 00`为依次为4个顶点坐标值，`EE 00`和`37 00`表示第一个顶点坐标为(238, 55)，`15 01`和`F7 FF`表示第二个顶点坐标为(277, -9)，`4E 01`和`19 00`表示第三个顶点坐标为(334, 25)，`27 01`和`5A 00`表示第四个顶点坐标为(295, 90)。


