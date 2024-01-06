# ベースはUbuntu20.04
FROM ubuntu:22.04

# apt-getのリポジトリを日本に変更 (オプション)
# RUN sed -i 's@archive.ubuntu.com@ftp.jaist.ac.jp/pub/Linux@g' /etc/apt/sources.list

# パッケージ更新
RUN apt-get update

# タイムゾーン設定 (オプション)
# RUN apt-get install -y tzdata
# ENV TZ=Asia/Tokyo

# C++開発環境
RUN apt-get install --no-install-recommends -y \
    g++ \
    gdb \
    make \
    clang-format \
    git && \ 
    apt-get autoremove -y && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*