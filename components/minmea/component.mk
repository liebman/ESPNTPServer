COMPONENT_OBJS := minmea/minmea.o
COMPONENT_SRCDIRS := minmea
COMPONENT_ADD_INCLUDEDIRS := minmea
CPPFLAGS += -Dtimegm=mktime
