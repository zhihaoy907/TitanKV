# ==========================================
# Week 1: Tests (Normal & Debug)
# ==========================================

# 1. iouring 最基本的性能测试
add_executable(test_iouring test_iouring.cpp)
target_link_libraries(test_iouring PRIVATE titankv_core)

# 2. 基于io_uring cpu binding 的多线程性能测试
add_executable(test_mutithread test_mutithread.cpp)
target_link_libraries(test_mutithread PRIVATE titankv_core)

# 3. 基于SPSCQueue的性能测试
add_executable(test_SPSCQueue test_SPSCQueue.cpp)
target_link_libraries(test_SPSCQueue PRIVATE titankv_core)
