TEMPLATE = app

QT += quick qml
SOURCES += main.cpp
RESOURCES += pathitem.qrc
OTHER_FILES += content/pathitem.qml \
               content/pathitemgallery.qml \
               content/pathiteminteract.qml \
               content/tiger.qml \
               content/item1.qml \
               content/item2.qml \
               content/item4.qml \
               content/item5.qml \
               content/item6.qml \
               content/item7.qml \
               content/item8.qml \
               content/item9.qml \
               content/item10.qml \
               content/item11.qml \
               content/item12.qml \
               content/item13.qml \
               content/item14.qml \
               content/item15.qml \
               content/item17.qml

target.path = $$[QT_INSTALL_EXAMPLES]/quick/pathitem
INSTALLS += target
