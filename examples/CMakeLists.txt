if (${Threads_FOUND})
  add_executable(example simple.cpp)
  target_link_libraries(example PRIVATE multiqueue_internal)
  target_link_libraries(example PRIVATE Threads::Threads)
endif()
