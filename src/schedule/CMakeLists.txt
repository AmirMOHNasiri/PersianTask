cmake_minimum_required (VERSION 3.22...4.0)
include_directories (${CMAKE_SOURCE_DIR}
                     ${CMAKE_SOURCE_DIR}/src
                     ${CMAKE_SOURCE_DIR}/src/commands
                     ${CMAKE_SOURCE_DIR}/src/columns
                     ${CMAKE_SOURCE_DIR}/src/schedule
                     ${CMAKE_SOURCE_DIR}/src/libshared/src
                     ${CMAKE_CURRENT_BINARY_DIR}
                     ${TASK_INCLUDE_DIRS})

# The example template is built in, so that 'task schedule init' works even
# where the documentation is not installed.
set (SCHEDULE_EXAMPLE_FILE ${CMAKE_SOURCE_DIR}/examples/schedule/student-week)
set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SCHEDULE_EXAMPLE_FILE})
file (READ ${SCHEDULE_EXAMPLE_FILE} SCHEDULE_EXAMPLE)
configure_file (ScheduleExample.h.in ${CMAKE_CURRENT_BINARY_DIR}/ScheduleExample.h @ONLY)

set (schedule_SRCS CmdSchedule.cpp       CmdSchedule.h
                   CmdScheduleEdit.cpp
                   ScheduleEffort.cpp    ScheduleEffort.h
                   ScheduleHelp.cpp      ScheduleHelp.h
                   SchedulePlanner.cpp   SchedulePlanner.h
                   ScheduleStartup.h
                   ScheduleState.cpp     ScheduleState.h
                   ScheduleTemplate.cpp  ScheduleTemplate.h
                   ScheduleView.cpp      ScheduleView.h)

add_library (schedule STATIC ${schedule_SRCS})
target_link_libraries(schedule taskchampion-cpp)
