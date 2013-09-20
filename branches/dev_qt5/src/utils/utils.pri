HEADERS  = utilsexport.h \
           jid.h \
           versionparser.h \
           xmpperror.h \
           stanza.h \
           action.h \
           menu.h \
           unzipfile.h \
           message.h \
           iconsetdelegate.h \
           toolbarchanger.h \
           datetime.h \
           filestorage.h \
           iconstorage.h \
           menubarchanger.h \
           statusbarchanger.h \
           ringbuffer.h \
           widgetmanager.h \
           options.h \
           shortcuts.h \
           systemmanager.h \
           textmanager.h \
           animatedtextbrowser.h \
           closebutton.h \
           searchlineedit.h \
           imagemanager.h \
           advanceditem.h \
           advanceditemmodel.h \
           advanceditemdelegate.h \
           filecookiejar.h \
		   boxwidget.h

SOURCES  = jid.cpp \
           versionparser.cpp \
           xmpperror.cpp \
           stanza.cpp \
           action.cpp \
           menu.cpp \
           unzipfile.cpp \
           message.cpp \
           iconsetdelegate.cpp \
           toolbarchanger.cpp \
           datetime.cpp \
           filestorage.cpp \
           iconstorage.cpp \
           menubarchanger.cpp \
           statusbarchanger.cpp \
           ringbuffer.cpp \
           widgetmanager.cpp \
           options.cpp \
           shortcuts.cpp \
           systemmanager.cpp \
           textmanager.cpp \
           animatedtextbrowser.cpp \
           closebutton.cpp \
           searchlineedit.cpp \
           imagemanager.cpp \
           advanceditem.cpp \
           advanceditemmodel.cpp \
           advanceditemdelegate.cpp \
           filecookiejar.cpp \
		   boxwidget.cpp

unix:!mac:!haiku {
  HEADERS += x11info.h
  SOURCES += x11info.cpp
}