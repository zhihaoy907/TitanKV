# ==========================================
# Week 1: Tests (Normal & Debug)
# ==========================================

# 1. 正常版本测试 (链接 titankv_core)
add_executable(io_test io_test.cpp)
target_link_libraries(io_test PRIVATE titankv_core)

# 2. Debug/ASan 版本测试 (链接 titankv_core_debug)
add_executable(io_test_debug io_test.cpp)
target_link_libraries(io_test_debug PRIVATE titankv_core_debug)

add_executable(bench_io bench_io.cpp)
target_link_libraries(bench_io PRIVATE titankv_core)

add_executable(test_mutithread test_mutithread.cpp)
target_link_libraries(test_mutithread PRIVATE titankv_core)

add_executable(test_SPSCQueue test_SPSCQueue.cpp)
target_link_libraries(test_SPSCQueue PRIVATE titankv_core)



# ==========================================
# 3. 注册 CTest
# ==========================================
# 两个都注册，跑测试时会全跑一遍
add_test(NAME IoTest_Release COMMAND io_test)
add_test(NAME IoTest_Debug   COMMAND io_test_debug)

