---
title: MaixCDK
id: home_page
---

<div>
<script src="/static/css/tailwind.css"></script>
</div>

<style>
h2 {
    font-size: 1.6em;
    font-weight: 600;
    font-weight: bold;
}
#page_wrapper
{
    background: #f2f4f3;
}
.dark #page_wrapper
{
    background: #1b1b1b;
}
.md_page #page_content
{
    padding: 1em;
}
.md_page #page_content > div
{
    width: 100%;
    max-width: 100%;
    text-align: left;
}
h1 {
    font-size: 3em;
    font-weight: 600;
    margin-top: 0.67em;
    margin-bottom: 0.67em;
}
#page_content h2 {
    font-size: 1.6em;
    font-weight: 600;
    margin-top: 1em;
    margin-bottom: 0.67em;
    font-weight: bold;
    text-align: center;
    margin-top: 3em;
    margin-bottom: 1.5em;
}
#page_content h3 {
    font-size: 1.5em;
    font-weight: 400;
    margin-top: 0.5em;
    margin-bottom: 0.5em;
}
#tags > p {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    padding: 1em;
}
#tags > p a {
    margin: 0.2em 0.2em;
}
#feature video, #feature img {
    height: 15em;
}
.feature_item {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: space-between;
    margin: 1em;
    border: 1em solid white;
    background: white;
    border-radius: 0.5em;
    overflow: hidden;
    max-width: 20em;
    box-shadow: 0 4px 6px -1px rgb(0 0 0 / 0.1), 0 2px 4px -2px rgb(0 0 0 / 0.1);
}
.dark .feature_item {
    border: 1em solid #2d2d2d;
    background: #2d2d2d;
}
.feature_item .feature {
    font-size: 1.2em;
    font-weight: 600;
}
.feature_item .description {
    font-size: 0.8em;
    font-weight: 400;
}
.feature_item video, .feature_item img {
    width: 100%;
    object-fit: cover;
}
.feature_item .img_video {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
}
.feature_item > div {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: space-between;
}
.feature_item p {
    padding: 0.5em;
}
#page_content li {
    margin: 0.5em;
    list-style-type: disc;
}
.white_border {
    border: 1em solid white;
}
.dark .white_border {
    border: 1em solid #2d2d2d;
}
.code-toolbar pre {
    margin: 0;
}
.code_wrapper {
    overflow: auto;
}
@media screen and (min-width: 1280px) {
    .md_page #page_content > div
    {
        width: 1440px;
        max-width: 1440px;
    }
}
@media screen and (max-width: 768px) {
    .code_wrapper {
        font-size: 0.6em;
    }
}
</style>

<!-- wrapper -->
<div class="flex flex-col justify-center items-center">

<div class="w-full flex flex-col justify-center text-center">
    <div class="flex justify-center">
        <img src="/static/image/maixcams.png" alt="MaixPy Banner">
    </div>
    <h1><span>MaixCDK</span></h1>
    <h3>快速实现 AI 视觉和听觉应用</h3>
</div>

<div id="big_btn_wrapper" class="flex flex-wrap justify-center items-center">
    <a class="btn m-1" href="/doc/zh/">快速开始 🚀📖</a>
    <a class="btn m-1" href="/api/">API 参考 📚</a>
    <a class="btn m-1" target="_blank" href="https://wiki.sipeed.com/maixcam-pro">硬件平台：MaixCAM 📷</a>
    <a class="btn m-1" target="_blank" href="https://github.com/sipeed/MaixCDK">开源代码 ⭐️</a>
    <a class="btn m-1" target="_blank" href="https://maixhub.com/app">应用商店 📦</a>
</div>

<div id="tags">

[![GitHub Repo stars](https://img.shields.io/github/stars/sipeed/MaixCDK?style=social)](https://github.com/sipeed/MaixCDK)[![Apache 2.0](https://img.shields.io/badge/license-Apache%20v2.0-orange.svg)]("https://github.com/sipeed/MaixCDK/blob/main/LICENSE.md)[![GitHub downloads](https://img.shields.io/github/downloads/sipeed/maixcdk/total?label=GitHub%20downloads)](https://github.com/sipeed/MaixCDK) [![Build MaixCAM](https://github.com/sipeed/MaixCDK/actions/workflows/build_maixcam.yml/badge.svg)](https://github.com/sipeed/MaixCDK/actions/workflows/build_maixcam.yml)[![Trigger wiki](https://github.com/sipeed/MaixCDK/actions/workflows/trigger_wiki.yml/badge.svg)](https://github.com/sipeed/MaixCDK/actions/workflows/trigger_wiki.yml)

</div>

<div class="text-center">

[English](../) | 中文

</div>


<div class="mt-16"></div>

<div class="text-gray-400 text-center">


如果您喜欢 MaixCDK 或 MaixPy，请为 [MaixPy](https://github.com/sipeed/MaixPy) 和 [MaixCDK](https://github.com/sipeed/MaixCDK) 开源项目点赞 ⭐️，以鼓励我们开发更多功能。

</div>


<div class="mt-6"></div>

<h2 class="text-center font-bold">简单的 API 设计，20 行代码实现 AI 图像识别</h2>
<div id="id1" class="flex flex-row justify-center items-end flex-wrap max-w-full">
<div class="shadow-xl code_wrapper">

```cpp
#include "maix_nn.hpp"
#include "maix_camera.hpp"
#include "maix_display.hpp"

int main()
{
    nn::Classifier classifier("/root/models/mobilenetv2.mud");
    image::Size input_size = classifier.input_size();
    camera::Camera cam = camera::Camera(input_size.width(), input_size.height(), classifier.input_format());
    display::Display disp = display::Display();

    char msg[64];
    while(!app::need_exit())
    {
        image::Image *img = cam.read();
        auto *result = classifier.classify(*img);
        int max_idx = result->at(0).first;
        float max_score = result->at(0).second;
        snprintf(msg, sizeof(msg), "%5.2f: %s", max_score, classifier.labels[max_idx].c_str());
        img->draw_string(10, 10, msg, image::COLOR_RED);
        disp.show(*img);
        delete result;
        delete img;
    }
    return 0;
}
```

</div>
<video playsinline controls autoplay loop muted preload  class="p-0 mx-2 rounded-md shadow-xl white_border" src="https://wiki.sipeed.com/maixpy/static/video/classifier.mp4" type="video/mp4">
分类结果视频
</video>
</div> <!-- id1 -->


<h2 class="text-center font-bold">同样简单的 Python API 绑定</h2>
<div id="id2" class="flex flex-row justify-center items-end flex-wrap max-w-full">
<div class="shadow-xl code_wrapper">

```python
from maix import camera, display, image, nn

classifier = nn.Classifier(model="/root/models/mobilenetv2.mud")
cam = camera.Camera(classifier.input_width(), classifier.input_height(), classifier.input_format())
disp = display.Display()

while 1:
    img = cam.read()
    res = classifier.classify(img)
    max_idx, max_prob = res[0]
    msg = f"{max_prob:5.2f}: {classifier.labels[max_idx]}"
    img.draw_string(10, 10, msg, image.COLOR_RED)
    disp.show(img)
```

</div>
<video playsinline controls autoplay loop muted preload  class="p-0 mx-2 rounded-md shadow-xl white_border" src="https://wiki.sipeed.com/maixpy/static/video/classifier.mp4" type="video/mp4">
分类结果视频
</video>
</div> <!-- id2 -->

<!-- div start-->
<div class="text-center flex flex-col justify-center items-center">
<h2>多样的内置功能</h2>

<p>MaixCDK 的功能与 MaixPy 保持同步，MaixPy 的文档也适用于 MaixCDK。</p>

<div style="display: flex; justify-content: left">
    <a target="_blank" style="margin: 1em;color: white; font-size: 0.9em; border-radius: 0.3em; padding: 0.5em 2em; background-color: #a80202" href="https://wiki.sipeed.com/maixpy/">请访问 MaixPy 以了解更多功能</a>
</div>

</div>
<!-- div end-->



<!-- div start-->
<div class="flex flex-col justify-center items-center max-w-full">
<h2>一秒添加 Python 绑定</h2>

只需添加注释，即可在 MaixPy 中使用此 API！

<div class="flex flex-row justify-center items-center flex-wrap mt-3 max-w-full">
<div class="mr-4 mt-4 shadow-xl code_wrapper flex flex-col justify-center items-center">

Python 示例代码：

```python
from maix import uart


devices = uart.list_devices()
print(devices)



```

</div>
<div class="max-w-full">
<div class="mt-4 shadow-xl code_wrapper flex flex-col justify-center items-center">

MaixCDK 在 `maix_uart.hpp` 中的实现：

```cpp
namespace maix::uart {
    /**
     * List all UART devices can be directly used.
     * @return All device path, list type, element is string type
     * @maixpy maix.uart.list_devices
     */
    std::vector<std::string> list_devices();
}
```

</div>
</div>
</div>
</div>
<!-- div end-->

<!-- start -->
<div class="flex flex-col justify-center items-center">
<h2>在线 AI 训练平台 MaixHub</h2>

无需 AI 专业知识或昂贵的训练设备，一键训练模型，一键部署到 MaixCAM。

<div class="mt-3"></div>

<img class="shadow-xl white_border" src="/static/image/maixhub.jpg">
</div>
<!-- end -->

## Maix 生态系统

<img src="/static/image/maix_ecosystem.png" class="white_border shadow-xl rounded-md">


## 社区 {#community}

<div class="max-w-full">
<div class="overflow-auto">

| 社区 | 地址 |
| --- | ---- |
| **MaixCDK 文档**| [MaixCDK 文档](https://wiki.sipeed.com/maixcdk/) |
| **MaixPy 文档**| [MaixPy 文档](https://wiki.sipeed.com/maixpy/) |
| **应用商店**| [maixhub.com/app](https://maixhub.com/app) |
| **项目分享**| [maixhub.com/share](https://maixhub.com/share) |
| **Bilibili**| 在 Bilibili 上搜索 `MaixCAM` 或 `MaixPy` |
| **讨论**| [maixhub.com/discussion](https://maixhub.com/discussion) |
| **MaixCDK 问题**| [github.com/sipeed/MaixPy/issues](https://github.com/sipeed/MaixCDK/issues) |
| **Telegram**| [t.me/maixpy](https://t.me/maixpy) |
| **QQ 群**| 862340358 |

</div>
</div>

</div>
<!-- wrapper end -->
