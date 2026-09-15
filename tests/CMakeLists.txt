add_executable(BandageTests bandagetests.cpp)
add_test(NAME BandageTests COMMAND BandageTests)

target_link_libraries(BandageTests PRIVATE BandageCLI BandageLib OGDF Qt6::Widgets Qt6::Test CLI11::CLI11)

# Throwaway feasibility spike for the gfaidx integration - not part of ctest,
# built/run manually. See tests/gfaidx_spike.cpp for what it checks.
if (BANDAGE_HAVE_GFAIDX)
  add_executable(GfaidxSpike EXCLUDE_FROM_ALL gfaidx_spike.cpp)
  target_link_libraries(GfaidxSpike PRIVATE BandageLib Qt6::Widgets)
  target_compile_definitions(GfaidxSpike PRIVATE
    GFAIDX_CLI_PATH="$<TARGET_FILE:gfaidx_cli>")
  add_dependencies(GfaidxSpike gfaidx_cli)
endif()
