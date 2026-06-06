# GIF背包

## 硬件

### BOM

|  item   | link  | 价格 |
|  ----  | ----  | --- |
| 背包  | [链接](https://item.taobao.com/item.htm?id=910082491605&pisk=g8Y_GUcrMKLEJ-KRfZlUPqx8D7_ffXurkS1vZIUaMNQOcrdwFszV0cfbl9O-sOomjKtBHpjNBtSVls9PKCzqsCljlIdRBZ-NQspehpqwQEW2LqOkFGzwMEPMxLR87FoGur_GoZHrU4uzjCbcks9OCFwgvs1juoUO6NbLUONZ24uysBNN6bRKzEkcMhC3DrpAXMFdZ9CTXtQxOwCN1ZUODPELp9fAkZIYkJQdMseO6ZpxJwCcM-CAHlQLvsf5k1pAXkhCi9COkZpv9X1mwMMCGc6w1bMAY143U6ROdrUvWqjOFhr4lr9C6G9vB9OeTe11fTSxUbeDPQd9rsTmwo6XnhpNtKgSdaKkdEsRlqah7QtpCMtshysHvQYAAHD7qQAvZnIBlRhwepKXH6xnho6WDC-k8MPYvGKkQ3IB8AaheHdBgMtnilW2qspcqEkuSOdvZUxPPVwl1Hd5Rgr_zTZ3BoNCqr1CUXGQmojmb9QnNpyF7GCh1UlIOREcX6fBTXGQmojOt16EOXwTm&spm=tbpc.boughtlist.suborder_itemtitle.1.6c262e8dxxPRiE&skuId=5772377368135) | ¥59.9 |
| esp32  | [链接](https://item.taobao.com/item.htm?id=721013912106&pisk=gOZ0T60jPbOB_1KXoOmfl57eWvQ8Gmi_b5Kt6chNzbl5CELAhVkaI5qTkRltj5Vgsj3NGmEgZJwOhlBj0GDa97mTHjhTZOVYChCf5mKwsSwFG-hOhlcZZSPi5tGt_fVTQrBRvMebhciaS6Idvqdo7DNDunlNQYkKBTHVFrSa8ciN91YJbqsnfSokkCvwzYlSIARZ_5oy4vMZbAuZQTcrIvTwg5oNETDi3IkqgcRr4AGygAuqb0lrHADZ3ckNE8lSQcla_qWuUbMZbflopZl3buEPs-bx2EMfHlDmoXyqg-eb4zAKO-PWXhnmiqYY3bxwbuaGWPlStgArwRyQ74qGkEi_eyVg7W5JkjznKWDLMg-mbzPux2y5tnG0zSZiD2sDOx4zK0lxYwYom8FbxmrhinH-Vj3U8-5JrY4zHu08Y6xilzPbRVqOwMiYJJEs8oSDOb3II70Lo6x0TgJ2zeyx0hMPBu865qkSEXBK5CIx7BkNVTXkROgqFxhdETY1hqkSEXBlEeOjuYMx9&spm=tbpc.boughtlist.suborder_itemtitle.1.6c262e8dxxPRiE) | ¥15 |
| 杜邦线 | None | ~¥0 |
| 固定用胶带/胶水 | None | ~¥0 |

### 硬件修改方式

把背包原有主控焊下来，按照 [clockwise](https://github.com/jnthas/clockwise/blob/gh-pages/static/images/display_esp32_wiring_bb.png) 方式接线、固定即可。

## 固件安装方式

### binary

TBD

### from source

使用PlatformIO进行编译以及update。
* build & upload
* build Filesystem Image & upload Filesystem Image

## 使用方法

1. 连接Wi-Fi，默认为`gif_backpack`，密码是`12345678`
2. 手机访问 `http://192.168.1.1/`
3. 在网页里选择 GIF 轨道；可以选具体 GIF，也可以选择“无动画”，只显示歌词
4. 在歌曲列表里搜索并选择歌曲
5. 点击歌词行可以把设备进度同步到该句歌词
6. 可以调整歌词颜色、第一行 Y 坐标、第二行 Y 坐标和亮度

## 歌词显示

当前实现把 GIF 和歌词解耦成两条轨道：

* GIF 轨道：继续读取 `/gif/*.gif`，可为空
* 歌词轨道：读取 `/lyrics/lyrics.pack` 中的当前歌曲，按 `/lyrics/index.txt` 的 offset/length 定位
* 字库：`/lyrics/font.bin` + `/lyrics/charset.txt`，混合宽度 1bpp；汉字 `16x16`，英文/数字/标点 `8x16`
* 渲染：每帧先画 GIF 背景，再叠加两行歌词；第二行显示当前句未来窗口，不够时提前露出下一句

Mayday.Blue / MayScreen 歌词资产已放在 `data/lyrics/`：

* `lyrics.pack`：198 首歌词打包后的文本
* `index.json`：网页搜索和展示使用
* `index.txt`：固件按文件名查 offset/length 使用
* `font.bin` / `charset.txt`：共享小字库
* `summary.json`：资产统计

重新生成歌词资产：

```bash
python3 -m venv /tmp/gif_backpack_asset_venv
/tmp/gif_backpack_asset_venv/bin/python -m pip install pillow
/tmp/gif_backpack_asset_venv/bin/python tools/build_mayday_blue_assets.py
```
