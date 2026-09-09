cmake_minimum_required(VERSION 3.16)

# Componentes compartilhados entre os nós do projeto (ex.: can_bus).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(volante_painel_c3)

# Atualiza o pacote enxuto dist/ apos cada build (best-effort: nao quebra o build
# se faltar python). Veja ../package.py.
find_program(VOLANTE_PYTHON NAMES python python3)
if(VOLANTE_PYTHON)
    add_custom_command(
        TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
        COMMAND ${VOLANTE_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/../package.py" --quiet --zip
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
        COMMENT "Atualizando pacote dist/ (Volante)"
        VERBATIM)
endif()
