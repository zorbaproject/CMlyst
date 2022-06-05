FROM opensuse/tumbleweed

RUN zypper addrepo --no-check --refresh --name 'Cutelyst' 'https://download.opensuse.org/repositories/home:/buschmann23:/Cutelyst/openSUSE_Tumbleweed/home:buschmann23:Cutelyst.repo'
RUN zypper --gpg-auto-import-keys refresh 
RUN zypper --non-interactive install --auto-agree-with-licenses clearsilver  clearsilver-devel  cutelee6-qt5  cutelee6-qt5-devel  cutelyst3-qt5  cutelyst3-qt5-devel  cutelyst3-qt5-plugins  cutelystd3-qt5 libCutelyst3Qt5-3  libCutelyst3Qt5Authentication3 libCutelyst3Qt5CSRFProtection3  libCutelyst3Qt5EventLoopEpoll3 libCutelyst3Qt5Memcached3 libCutelyst3Qt5MemcachedSessionStore3 libCutelyst3Qt5Server3  libCutelyst3Qt5Session3 libCutelyst3Qt5StaticCompressed3 libCutelyst3Qt5StaticSimple3 libCutelyst3Qt5StatusMessage3 libCutelyst3Qt5UserAgent3 libCutelyst3Qt5UtilsLangSelect3 libCutelyst3Qt5UtilsPagination3  libCutelyst3Qt5UtilsSql3 libCutelyst3Qt5UtilsValidator3  libCutelyst3Qt5ViewClearSilver3  libCutelyst3Qt5ViewCutelee3  libCutelyst3Qt5ViewEmail3 libCutelyst3Qt5ViewGrantlee3  libCutelyst3Qt5ViewJson3  libSimpleMail2Qt5-0  libSimpleMail2Qt5-devel
RUN zypper --non-interactive install --auto-agree-with-licenses asql-migration libASqlQt5-0 libASqlQt5-devel libASqlQt5Pg0 libASqlQt6-0 libASqlQt6-devel libASqlQt6Pg0
RUN zypper --non-interactive install --auto-agree-with-licenses git w3m wget curl

# Build cmlyst
RUN mkdir -p /usr/local/src/cmlyst
COPY . /usr/local/src/cmlyst
RUN cd /usr/local/src/cmlyst \
    && cmake . \
    && make \
    && cmake --build . --target install

COPY ./docker/appserver/01_init.sh /01_init.sh
RUN chmod +x /01_init.sh
WORKDIR /usr/local/src/cmlyst

COPY ./docker/appserver/cmlyst.ini /usr/local/src/cmlyst/config.ini

ARG DATA_FOLDER
ENV DATA_FOLDER $DATA_FOLDER

RUN ln -s /usr/local/src/cmlyst/src/cmlystd /usr/bin/
RUN mkdir -p /usr/local/share/cmlyst
# Prepare config file
RUN sed -i -e "s&DataLocation.*&DataLocation = $DATA_FOLDER&g" /usr/local/src/cmlyst/config.ini
CMD ["/01_init.sh"]
#CMD ["cmlystd", "--http-socket", ":3000", "--ini","config.ini"]


