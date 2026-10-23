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
                   ScheduleAsk.cpp       ScheduleAsk.h
                   CmdScheduleEdit.cpp
                   CmdScheduleReview.cpp
                   ScheduleEffort.cpp    ScheduleEffort.h
                   ScheduleHelp.cpp      ScheduleHelp.h
                   SchedulePlanner.cpp   SchedulePlanner.h
                   ScheduleStartup.h
                   ScheduleState.cpp     ScheduleState.h
                   ScheduleTemplate.cpp  ScheduleTemplate.h
                   ScheduleView.cpp      ScheduleView.h)

# Test-only switches, such as schedule.test.leak-now, exist only in test
# builds.  persiantask/build-and-test.sh turns this on; packages do not.
option (PERSIANTASK_TESTING "Build the PersianTask test-only switches into task" OFF)

add_library (schedule STATIC ${schedule_SRCS})
if (PERSIANTASK_TESTING)
  target_compile_definitions (schedule PRIVATE PERSIANTASK_TESTING)
endif (PERSIANTASK_TESTING)
target_link_libraries(schedule taskchampion-cpp)
