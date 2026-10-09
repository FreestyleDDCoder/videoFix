Untrunc
=======

Restore a damaged (truncated) mp4, m4v, mov, 3gp video. Provided you have a similar not broken video. And some luck.

You need:

* Another video file which isn't broken
* ~~Basic ability to use a command line~~ ([GUI](#GUI) exists)

## About this fork
This fork improves the [original](https://github.com/ponchio/untrunc) in the following:
* more than 10 times faster!
* low memory usage, fixes [#30](https://github.com/ponchio/untrunc/issues/30#issuecomment-143744821)
* easier to build + automated [windows builds](https://github.com/anthwlock/untrunc/releases/latest)
* \>2GB file support
* ability to skip over unknown bytes
* generic support for all tracks with fixed-width chunks (e.g. twos/sowt)
* advanced logging system
* can stretch/shrink video to match audio duration
* compatible with new versions of ffmpeg
* handles invalid atom lengths
* supports GoPro and Sony XAVC videos
* many bugs got fixed, actively maintained

## Building

Windows users can download the latest version [here](https://github.com/anthwlock/untrunc/releases/latest).\
In certain cases a specific version of ffmpeg is needed. Untrunc works great with ffmpeg 3.3.9.\
For FFmpeg versions > 8.1, struct definition mismatches can occur if upstream changes the internal `FFCodec` struct, potentially leading to undefined behavior (see details in [src/ff_internal.h](src/ff_internal.h)).

#### With system libraries

```shell
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev
# get the source code
make
sudo cp untrunc /usr/local/bin
```

#### With local libraries

Just use following commands, make will do the rest for you.

```shell
sudo apt-get install yasm wget
make FF_VER=3.3.9
sudo cp untrunc /usr/local/bin
```

#### GUI

The GUI is optional. It is included in the automated [windows builds](https://github.com/anthwlock/untrunc/releases/latest).\
You will need [libui](https://github.com/andlabs/libui). After that, just do

```shell
make untrunc-gui
```

#### CentOS 7

```shell
sudo yum -y install epel-release && sudo yum -y install git gcc-c++ yasm
git clone --depth 5 https://github.com/anthwlock/untrunc && cd untrunc
make FF_VER=3.3.9
sudo cp untrunc /usr/local/bin
```

#### macOS with Homebrew

```
brew install ffmpeg yasm
CPPFLAGS="-I/opt/homebrew/include" LDFLAGS="-L/opt/homebrew/lib" make
```


## Docker container

You can use the included Dockerfile to build and execute the package as a container.\
The optional argument 'FF_VER' will be passed to `make`.

```shell
# docker build --build-arg FF_VER=3.3.9 -t untrunc .
docker build -t untrunc .
docker image prune --filter label=stage=intermediate -f

docker run --rm -v ~/Videos/:/mnt untrunc /mnt/ok.mp4 /mnt/broken.mp4
```

## Snapcraft

If you have `snap`, you can use `sudo snap install --edge untrunc-anthwlock`.

[![untrunc-anthwlock](https://snapcraft.io//untrunc-anthwlock/badge.svg)](https://snapcraft.io/untrunc-anthwlock)

## Using

You need both the broken video and an example working video (ideally from the same camera, if not the chances to fix it are slim).

Run this command in the folder where you have unzipped and compiled Untrunc but replace the `/path/to/...` bits with your 2 video files:

```shell
./untrunc /path/to/working-video.m4v /path/to/broken-video.m4v
```

Then it should churn away and hopefully produce a playable file called `broken-video_fixed.m4v`.

That's it you're done!

(Thanks to Tom Sparrow for providing the guide)


---

# 修复原理与流程（技术文档）

## 一、项目定位

本工具用于修复**被截断/损坏的 MP4/MOV/M4V/3GP 容器文件**（典型场景：录制中断电、写入中断导致文件尾部不完整）。它修复的是**容器索引结构（moov atom）**，而不是码流本身的错误。

核心思路：**不重编码、不解码，只解析容器结构和码流明文头部，重建索引表**。因此修复速度极快——对一个几 GB 的文件只需几分钟，瓶颈主要在磁盘 I/O 而非 CPU。

## 二、两阶段修复流程

### 阶段 1：解析健康参考视频（learning）

入口：`Mp4::parseOk()`（src/mp4.cpp:106）

从同设备拍摄的正常视频中提取：

| 提取内容 | 用途 | 关键代码 |
|---|---|---|
| moov atom 结构、track 配置 | 损坏文件没有 moov，直接借用其容器骨架 | `Mp4::parseHealthy()`（src/mp4.cpp:46） |
| 编解码参数（SPS/PPS 等） | 判断帧类型与边界 | `src/avc1/`、`src/hvc1/` |
| 样本长度统计规律 | 推断每帧大小、辅助归属判断 | `src/mutual_pattern.cpp` |
| 音视频轨道交错模式 | 判断 mdat 中当前位置属于哪个 track | `Mp4::genChunkTransitions()`（src/mp4.cpp:702） |

### 阶段 2：扫描损坏文件并重建索引（repair）

入口：`Mp4::repair()`（src/mp4.cpp:2247）

主循环逻辑：

```
while (未到文件尾) {
    1. getMatch()：遍历所有 track，用参考视频学到的特征
       判断当前偏移处的数据属于哪个 track、是什么帧、有多长
       （Codec::matchSample()，src/codec.cpp:339）
    2. 命中 → 记录该样本的 (track, offset, size)，推进 offset
    3. 未命中 → 尝试跳过未知字节 / 回溯重试
}
最后：用重建的样本表生成 stco/stts/stsz 等索引 atom，
      拼装 moov + mdat 写出 *_fixed.mp4
```

### 为什么不需要解码

H.264/H.265 的 NAL unit 头部（1~2 字节）和 slice header 的明文部分是**自描述的二进制结构**：起始码（`00 00 00 01`）、nal_unit_type、frame_num 等可直接按位流解析。帧的**边界和类型**不需要熵解码即可获知；只有像素重建才需要解码。本工具通过 `getSizeAvc1()`（src/avc1/avc1.cpp）/`getSizeHvc1()`（src/hvc1/hvc1.cpp）做纯字节扫描确定帧边界与大小，全程不调用解码器（FFmpeg 仅用于容器轻量探测）。

### 内存与 I/O 模型

`BufferedAtom`（src/atom.cpp）流式读取 mdat，只在内存维护小窗口缓存，边扫描边构建索引，最后一次性写出——支持 >2GB 文件且内存占用极低。

## 三、参考视频的作用与局限

参考视频提供的是**还原完整度**，不是正确性：

- 有参考视频：音视频完整恢复，时长/交错结构准确（最佳效果）
- 无参考：视频 track 仍可恢复（帧边界是码流自描述的），但音频（AAC 无自描述边界特征）基本丢失，结果为无声视频
- 只知编码参数（分辨率/码率/GOP）：当前代码不利用此类先验，理论上可实现 CBR 启发式（按码率反推帧长做校验、利用 AAC 帧长恒定做音视频对齐），属于可行的改造方向

## 四、关键源码索引

| 文件 | 作用 |
|---|---|
| src/main.cpp | CLI 入口，命令解析 |
| src/mp4.cpp / .h | 核心流程：`parseOk()` / `repair()` / `saveVideo()` |
| src/codec.cpp | 编解码器匹配分发：`matchSample()` / `getSize()` |
| src/atom.cpp / .h | MP4 atom 二叉树解析与写入（BufferedAtom 流式缓存） |
| src/track.cpp | track 级 chunk/sample 管理 |
| src/avc1/ | H.264 帧边界检测（getSizeAvc1、nal-slice） |
| src/hvc1/ | H.265 帧边界检测（getSizeHvc1、nal-slice） |
| src/mutual_pattern.cpp | 跨文件字节模式匹配（参考视频样本长度规律） |
| src/rsv.cpp | Sony RSV 格式在写恢复 |
| src/untrunc_api.cpp / .h | 供外部（Android 库）调用的 C API 封装 |

## 五、当前可优化的方向

1. **CBR/GOP 先验启发式**：车载录制场景参数固定（分辨率/帧率/码率/GOP），可按码率反推帧长做校验，AAC 音频帧长恒定可用于无参考时的音视频对齐——降低对参考视频的依赖。
2. **匹配失败的回溯策略**：当前未命中时逐字节推进重试，损坏严重时会产生大量重复扫描；可引入更强的跳段策略或并行匹配。
3. **多线程扫描**：mdat 扫描本质串行（依赖前一帧归属推断），但不同 track 的候选匹配可并行，字节窗口预取可流水线化。
4. **Android 平台集成打磨**：`android-lib/` + `untrunc_api` 已具备基础，可补齐进度回调、取消机制与 64 位偏移边界测试。
5. **音频恢复增强**：对 AAC/PCM 之外的编码（如 aptX、Opus）增加自描述边界识别，提升无参考模式还原度。

---

### Help/Support

#### Reporting issues
Use the `-v` parameter for a more detailed output. Both the healthy and corrupt file might be needed to help you.

#### Donation
If this software helped you please consider donating [here](https://www.paypal.me/anthwlock)!\
Donations will encourage me to keep working on this software, leading to more media being supported and better recovered files.

You might also want to consider donating to **ponchio**, see his instructions [here](https://github.com/ponchio/untrunc#helpsupport).

Thank you.
