2006.08.05
successfully compiled under:
Visual Studio 6 Interprise Edition(vc6) 
Visual C++ 2005 professional(vc8)
Intel C++ Compiler 8.0

corresponding x264 version: Rev.551
Every modification is remarked with //lsp...
Any problem about my x264 for vc, please send me mail or submit the question on my blog: http://blog.csdn.net/sunshine1314

ps: x264 changelog
https://trac.videolan.org/x264/log/trunk/

【x264 cli常用选项解释】
1.最简单的方式
x264 -o test.264 infile.yuv widthxheight
example:
x264 -o test.264 foreman.cif 352x288
note: 352x288中间的是英文字母x，而不是*，总是有一些朋友会搞错了，所以我在此多一句提醒一下。

2.量化步长固定
--qp ??
example:
x264 --qp 26 -o test.264 foreman.cif 352x288

3.采用cavlc
--no-cabac 
note: 默认是采用cabac

4.设置编码帧数
--frames ?? 
example:
x264 --frames 10 --qp 26 -o test.264 foreman.cif 352x288

5. 设置参考帧数目
--ref ??
example:
x264 -- ref 2 --frames 10 --qp 26 -o test.264 foreman.cif 352x288

6. 用于dvdrip的经典选项：码率控制，采用B帧，两遍编码
example:
x264.exe --frames 10 --pass 1 --bitrate 1500 --ref 2 --bframes 2 --b-pyramid --b-rdo --bime --weightb --filter -2,-2 --trellis 1 --analyse all --8x8dct --progress -o test1.264 foreman.cif 352x288

x264.exe --frames 10 --pass 2 --bitrate 1500 --ref 2 --bframes 2 --b-pyramid --b-rdo --bime --weightb --filter -2,-2 --trellis 1 --analyse all --8x8dct --progress -o test2.264 foreman.cif 352x288

7. 显示编码进度
--progress



                ╭═══════════════╮
                ║                              ║
  ╭══════┤           Peter Lee          ├══════╮
  ║            ║                              ║            ║
  ║            ╰═══════════════╯            ║
　║                                                          ║
　║                                                          ║
　║　 I'am happy of talking with you about video coding,     ║
　║             transmission, vod and so on.                 ║
  ║       Welcom to MY homepage:  videosky.9126.com          ║
  ║                                                          ║
　║                                                          ║
  ║    ╭───────────────────────╮    ║
  ╰══┤               lspbeyond@126.com              ├══╯
        ╰───────────────────────╯

