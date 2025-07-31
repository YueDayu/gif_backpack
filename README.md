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
2. 修改Wi-Fi ssid&密码、更改亮度： 点击设置
3. 上传Gif： 点击上传； 无法上传过大文件、只支持分辨率64*64、不能重名且文件名需要小于16个字符
4. 更改显示图片： 点击列表中的`显示`
5. 删除图片： 点击列表中的`删除`
6. 关闭显示、开始显示： 按按钮
