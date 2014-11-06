LEVEL = ../../../make

CXX_SOURCES := main2.cpp

include $(LEVEL)/Makefile.rules

main2.cpp : main1.cpp
	$(CXX) -E -m64 -g -O0 main1.cpp -o main2.cpp

clean::
	$(RM) "$(EXE)" $(OBJECTS) $(PREREQS) $(ARCHIVE_NAME) $(ARCHIVE_OBJECTS)
	$(RM) -r *.d.[0-9] *.d.[0-9][0-9] *.d.[0-9][0-9][0-9] *.d.[0-9][0-9][0-9][0-9] *.d.[0-9][0-9][0-9][0-9][0-9]
ifneq "$(DSYM)" ""
	$(RM) -r "$(DSYM)"
endif
	$(RM) main2.cpp
