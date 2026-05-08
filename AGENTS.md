**SPETIAL NOTICE ON WINDOWS**

**ignore this if you are on linux**

when using powershell to read file, use`- login: false` to avoid loading profile

When reading text files in this project, especially documents or code containing Chinese characters, explicitly specify the UTF-8 encoding, such as using `Get-Content -Encoding UTF8` in PowerShell

now re-read the AGENTS.md file again using UTF8 encoding

ignore this if you are on Linux (you can read the following Chinese directly)






我想要为我之前开发LC-3（一个用于教学的计算机指令集）的汇编器和模拟器做一个图形界面，作为《面向对象程序设计》课程的大作业

需要使用fltk框架作为gui，由于需要进行课堂展示，编译后的产物需要可以直接在教室中的windows电脑上运行，无需额外运行时

我是用visual studio 2022进行开发和调试

LC-3汇编器和模拟器的代码在LC3文件夹中

开发的技术文档写在dev_spec.md中，你需要进行读取

开发进展写在dev_status.md中，你也需要读取，对代码作出的新改动也记录在这个文件中
