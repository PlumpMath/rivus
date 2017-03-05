cmake_minimum_required(VERSION 3.5)

PROJECT(RIVUS)

SET(EXECUTABLE_OUTPUT_PATH build/)

INCLUDE_DIRECTORIES(include)

AUX_SOURCE_DIRECTORY(src SRC_RIVUS)

ADD_LIBRARY(rivus ${SRC_RIVUS})

ADD_EXECUTABLE(client example/client.c)
ADD_EXECUTABLE(server example/server.c)
ADD_EXECUTABLE(cond_eg example/cond_eg.c)
ADD_EXECUTABLE(mutex_eg example/mutex_eg.c)
ADD_EXECUTABLE(sem_eg example/sem_eg.c)
ADD_EXECUTABLE(rwlock_eg example/rwlock_eg.c)

TARGET_LINK_LIBRARIES(rivus pthread)
TARGET_LINK_LIBRARIES(client rivus)
TARGET_LINK_LIBRARIES(server rivus)
TARGET_LINK_LIBRARIES(cond_eg rivus)
TARGET_LINK_LIBRARIES(mutex_eg rivus)
TARGET_LINK_LIBRARIES(sem_eg rivus)
TARGET_LINK_LIBRARIES(rwlock_eg rivus)
