# C语言编辑器
## 文件结构
edictor.h ：主要接口

input.c ：处理输入

buffer: 处理缓冲内存

core: 核心编辑器

file: 文件io

status：状态栏

rows：行管理

rawmode.c: 终端原始模式设置

## 解题思路
参考了一下kilo编辑器, 然后自己尝试手写了一些
https://gitcode.com/gh_mirrors/ki/kilo-src/tree/master

另外一些代码
https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html

主要就是顺着几个level来，但是有一些磕磕绊绊的，总有一起奇怪的bug

## 