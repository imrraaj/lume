#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

#include "ast.hpp"
#include "gen.hpp"
#include "parse.hpp"
#include "token.hpp"

int main(int argc, const char* argv[])
{
    if (argc < 2) {
        std::cerr << "[Error] Invalid number of arguments\n";
        std::cout << "[Usage] lume <input.lm>\n";
        exit(EXIT_FAILURE);
    }

    bool verbose = true;

    const std::filesystem::path input_file_path = argv[1];
    const std::string input_filename = input_file_path.stem();
    const std::filesystem::path input_dir = input_file_path.root_directory();
    const std::filesystem::path output_dir = "./";

    if (verbose) {
        std::cout << "[Info] Compiling: " << input_file_path.string() << std::endl;
        std::cout << "[Progress] Tokenizing" << std::endl;
    }

    std::vector<Token> file_tokens = tokenize_file(input_file_path);
    if (verbose) {
        std::cout << "[Progress] Parsing" << std::endl;
    }

    Parser parser(std::move(file_tokens));

    const auto ret = parser.parse_block();
    if (!ret.has_value()) {
        std::cerr << "[Error] Expected statement list" << std::endl;
        exit(EXIT_FAILURE);
    }
    const ast::NodeBlock* root = ret.value();

    if (verbose) {
        std::cout << "[Progress] Generating" << std::endl;
    }
    {
        std::fstream file((output_dir / (input_filename + ".asm")), std::ios::out);

        Generator gen(file);
        gen.print_u64_def();
        gen.print_i64_def();
        gen.start();
        {
            gen.ast_block(root);
            gen.exit();
        }
        gen.append_data();
    }

    if (verbose) {
        std::cout << "[Progress] Assembling" << std::endl;
    }
    std::stringstream nasm_cmd;
    nasm_cmd << "nasm -felf64 " << output_dir / input_filename << ".asm";
    int return_val = system(nasm_cmd.str().c_str());

    if (return_val != 0) {
        std::cerr << "[Error] Assembling failed\n";
        exit(EXIT_FAILURE);
    }

    if (verbose) {
        std::cout << "[Progress] Linking" << std::endl;
    }
    std::stringstream ld_cmd;
    ld_cmd << "ld " << output_dir / input_filename << ".o -o " << output_dir / input_filename;
    return_val = system(ld_cmd.str().c_str());

    if (return_val != 0) {
        std::cerr << "[Error] Linking failed\n";
        exit(EXIT_FAILURE);
    }

    if (verbose) {
        std::cout << "[Progress] Done" << std::endl;
    }
    // remove asm and object file from the directory
    std::filesystem::remove(output_dir / (input_filename + ".asm"));
    std::filesystem::remove(output_dir / (input_filename + ".o"));

    return EXIT_SUCCESS;
}