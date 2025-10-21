#include "main.h"
#include "assembler/assembler.h"
#include "utils.h"
#include "globals.h"
#include "vm/rvss/rvss_vm.h"
#include "vm/rv5s/rv5s_vm.h"
#include "vm_runner.h"
#include "command_handler.h"
#include "config.h"

#include <iostream>
#include <thread>
#include <bitset>
#include <regex>
#include <utility>



int main(int argc, char *argv[]) {
  if (argc <= 1) {
    std::cerr << "No arguments provided. Use --help for usage information.\n";
    return 1;
  }

  bool start_vm_requested = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--help" || arg == "-h") {
        std::cout << "Usage: " << argv[0] << " [options]\n"
                  << "Options:\n"
                  << "  --help, -h           Show this help message\n"
                  << "  --assemble <file>    Assemble the specified file\n"
                  << "  --run <file>         Run the specified file\n"
                  << "  --verbose-errors     Enable verbose error printing\n"
                  << "  --start-vm           Start the VM with the default program\n"
                  << "  --start-vm --vm-as-backend  Start the VM with the default program in backend mode\n"
                  << "  --vm-mode <1..6|name>  Set pipeline mode for this run (1=single, 2=nohaz, 3=stall, 4=fwd, 5=static, 6=dyn1)\n"
                  << "  (VM core is auto-selected from pipeline_mode; you can also use: modify_config Execution pipeline_mode <1..6>)\n";
        return 0;

    } else if (arg == "--assemble") {
        if (++i >= argc) {
            std::cerr << "Error: No file specified for assembly.\n";
            return 1;
        }
        try {
            AssembledProgram program = assemble(argv[i]);
            std::cout << "Assembled program: " << program.filename << '\n';
            return 0;
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << '\n';
            return 1;
        }

  } else if (arg == "--run") {
        if (++i >= argc) {
            std::cerr << "Error: No file specified to run.\n";
            return 1;
        }
        try {
            AssembledProgram program = assemble(argv[i]);
            std::unique_ptr<VmBase> vm;
            auto mode = vm_config::config.getPipelineMode();
            if (mode == vm_config::PipelineMode::SINGLE_CYCLE) {
              vm = std::make_unique<RVSSVM>();
              std::cout << "VM_CORE: SINGLE_STAGE\n";
            } else {
              vm = std::make_unique<RV5SVM>();
              std::cout << "VM_MODE: MULTI_STAGE\n";
            }
            vm->LoadProgram(program);
            vm->Run();
            std::cout << "Program running: " << program.filename << '\n';
            return 0;
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << '\n';
            return 1;
        }

    } else if (arg == "--verbose-errors") {
        globals::verbose_errors_print = true;
        std::cout << "Verbose error printing enabled.\n";

    } else if (arg == "--vm-as-backend") {
        globals::vm_as_backend = true;
        std::cout << "VM backend mode enabled.\n";
  } else if (arg == "--start-vm") {
        // Don't break; keep parsing subsequent args like --vm-mode regardless of order
        start_vm_requested = true;
        continue;
  } else if (arg == "--vm-mode") {
    if (++i >= argc) {
      std::cerr << "Error: No mode specified for --vm-mode. Use 1..6 or names (single,nohaz,stall,fwd,static,dyn1).\n";
      return 1;
    }
    std::string m = argv[i];
    using vm_config::PipelineMode;
    auto setMode = [&](PipelineMode pm){ vm_config::config.setPipelineMode(pm); };
    if (m == "1" || m == "single" || m == "single_cycle") setMode(PipelineMode::SINGLE_CYCLE);
    else if (m == "2" || m == "nohaz" || m == "pipe_no_haz" || m == "no_hazard") setMode(PipelineMode::PIPE_NO_HAZ);
    else if (m == "3" || m == "stall" || m == "pipe_stall") setMode(PipelineMode::PIPE_STALL);
    else if (m == "4" || m == "fwd" || m == "forward" || m == "pipe_fwd") setMode(PipelineMode::PIPE_FWD);
    else if (m == "5" || m == "static" || m == "static_bp" || m == "pipe_static_bp") setMode(PipelineMode::PIPE_STATIC_BP);
    else if (m == "6" || m == "dyn1" || m == "onebit" || m == "pipe_dyn1_bp" || m == "dynamic_1bit") setMode(PipelineMode::PIPE_DYN1_BP);
    else {
      std::cerr << "Unknown --vm-mode value: " << m << ". Use 1..6 or names (single,nohaz,stall,fwd,static,dyn1).\n";
      return 1;
    }
    std::cout << "VM_MODE_SET " << m << "\n";
    continue;
    } else {
        std::cerr << "Unknown option: " << arg << '\n';
        return 1;
    }
  }
  
  

  setupVmStateDirectory();



  AssembledProgram program;
  bool program_loaded = false;
  std::unique_ptr<VmBase> vm;
  {
    auto mode = vm_config::config.getPipelineMode();
    if (mode == vm_config::PipelineMode::SINGLE_CYCLE) {
      vm = std::make_unique<RVSSVM>();
      std::cout << "VM_CORE: SINGLE_STAGE" << std::endl;
    } else {
      vm = std::make_unique<RV5SVM>();
      std::cout << "VM_CORE: MULTI_STAGE" << std::endl;
    }
  }
  // try {
  //   program = assemble("/home/vis/Desk/codes/assembler/examples/ntest1.s");
  // } catch (const std::runtime_error &e) {
  //   std::cerr << e.what() << '\n';
  //   return 0;
  // }

  // std::cout << "Program: " << program.filename << std::endl;

  // unsigned int count = 0;
  // for (const uint32_t &instruction : program.text_buffer) {
  //     std::cout << std::bitset<32>(instruction)
  //               << " | "
  //               << std::setw(8) << std::setfill('0') << std::hex << instruction
  //               << " | "
  //               << std::setw(0) << count
  //               << std::dec << "\n";
  //     count += 4;
  // }

  // vm.LoadProgram(program);
  

  std::cout << "VM_STARTED" << std::endl;
  // std::cout << globals::invokation_path << std::endl;

  std::thread vm_thread;
  std::atomic<bool> vm_running{false};

  auto launch_vm_thread = [&](auto fn) {
    if (vm_thread.joinable()) {
      vm->RequestStop();   
      vm_thread.join();
    }
    vm_running = true;
    vm->ClearStop();
    vm_thread = std::thread([fn = std::move(fn), &vm_running]() mutable {
      fn();
      vm_running = false;
    });
  };

  auto ensureVmMatchesMode = [&]() {
    using vm_config::PipelineMode;
    auto desired = vm_config::config.getPipelineMode();
    bool needSingle = (desired == PipelineMode::SINGLE_CYCLE);
    bool isSingle = (dynamic_cast<RVSSVM*>(vm.get()) != nullptr);
    if ((needSingle && !isSingle) || (!needSingle && isSingle)) {
      // Stop any running VM
      if (vm_thread.joinable()) {
        vm->RequestStop();
        vm_thread.join();
        vm_running = false;
      }
      // Recreate VM of correct kind
      if (needSingle) {
        vm = std::make_unique<RVSSVM>();
        std::cout << "VM_CORE: SINGLE_STAGE" << std::endl;
      } else {
        vm = std::make_unique<RV5SVM>();
        std::cout << "VM_CORE: MULTI_STAGE" << std::endl;
      }
      if (program_loaded) {
        vm->LoadProgram(program);
      }
    }
  };

  std::string command_buffer;
  while (true) {
    // std::cout << "=> ";
    if (!std::getline(std::cin, command_buffer)) {
      // EOF or stream error: exit cleanly
      break;
    }
    if (command_buffer.empty()) {
      continue;
    }
    command_handler::Command command = command_handler::ParseCommand(command_buffer);

  if (command.type==command_handler::CommandType::MODIFY_CONFIG) {
      if (command.args.size() != 3) {
        std::cout << "VM_MODIFY_CONFIG_ERROR" << std::endl;
        continue;
      }
      try {
    vm_config::config.modifyConfig(command.args[0], command.args[1], command.args[2]);
        std::cout << "VM_MODIFY_CONFIG_SUCCESS" << std::endl;
      } catch (const std::exception &e) {
        std::cout << "VM_MODIFY_CONFIG_ERROR" << std::endl;
        std::cerr << e.what() << '\n';
        continue;
      }
      // If pipeline_mode changed, align VM type for next execution
      if (command.args[0] == "Execution" && command.args[1] == "pipeline_mode") {
        // Only recreate immediately if not running; otherwise will switch on next RUN/STEP
        if (!vm_running) {
          ensureVmMatchesMode();
        }
      }
      continue;
    }
    if (command.type==command_handler::CommandType::LOAD) {
      try {
        if (vm_thread.joinable()) {
          vm->RequestStop();
          vm_thread.join();
          vm_running = false;
        }
        program = assemble(command.args[0]);
        std::cout << "VM_PARSE_SUCCESS" << std::endl;
        vm->output_status_ = "VM_PARSE_SUCCESS";
        vm->DumpState(globals::vm_state_dump_file_path);
      } catch (const std::runtime_error &e) {
        std::cout << "VM_PARSE_ERROR" << std::endl;
        vm->output_status_ = "VM_PARSE_ERROR";
        vm->DumpState(globals::vm_state_dump_file_path);
        std::cerr << e.what() << '\n';
        continue;
      }
  vm->LoadProgram(program);
  program_loaded = true;
      std::cout << "Program loaded: " << command.args[0] << std::endl;
    } else if (command.type==command_handler::CommandType::RUN) {
      if (vm_running) continue;
  ensureVmMatchesMode();
      launch_vm_thread([&]() { vm->Run(); });
    } else if (command.type==command_handler::CommandType::DEBUG_RUN) {
      if (vm_running) continue;
  ensureVmMatchesMode();
      launch_vm_thread([&]() { vm->DebugRun(); });
    } else if (command.type==command_handler::CommandType::STOP) {
      vm->RequestStop();
      if (vm_thread.joinable()) {
        vm_thread.join();
        vm_running = false;
      }
      std::cout << "VM_STOPPED" << std::endl;
  vm->output_status_ = "VM_STOPPED";
  vm->DumpState(globals::vm_state_dump_file_path);
    } else if (command.type==command_handler::CommandType::STEP) {
      if (vm_running) continue;
  ensureVmMatchesMode();
  launch_vm_thread([&]() { vm->Step(); });

    } else if (command.type==command_handler::CommandType::UNDO) {
      if (vm_running) continue;
  vm->Undo();
    } else if (command.type==command_handler::CommandType::REDO) {
      if (vm_running) continue;
  vm->Redo();
    } else if (command.type==command_handler::CommandType::RESET) {
      if (vm_running) continue;
      vm->Reset();
    } else if (command.type==command_handler::CommandType::EXIT) {
  vm->RequestStop();
      if (vm_thread.joinable()) vm_thread.join(); // ensure clean exit
  vm->output_status_ = "VM_EXITED";
  vm->DumpState(globals::vm_state_dump_file_path);
      break;
    } else if (command.type==command_handler::CommandType::ADD_BREAKPOINT) {
  vm->AddBreakpoint(std::stoul(command.args[0], nullptr, 10));
    } else if (command.type==command_handler::CommandType::REMOVE_BREAKPOINT) {
  vm->RemoveBreakpoint(std::stoul(command.args[0], nullptr, 10));
    } else if (command.type==command_handler::CommandType::MODIFY_REGISTER) {
      try {
        if (command.args.size() != 2) {
          std::cout << "VM_MODIFY_REGISTER_ERROR" << std::endl;
          continue;
        }
        std::string reg_name = command.args[0];
        uint64_t value = std::stoull(command.args[1], nullptr, 16);
  vm->ModifyRegister(reg_name, value);
  DumpRegisters(globals::registers_dump_file_path, vm->registers_);
        std::cout << "VM_MODIFY_REGISTER_SUCCESS" << std::endl;
      } catch (const std::out_of_range &e) {
        std::cout << "VM_MODIFY_REGISTER_ERROR" << std::endl;
        continue;
      } catch (const std::exception& e) {
        std::cout << "VM_MODIFY_REGISTER_ERROR" << std::endl;
        continue;
      }
    } else if (command.type==command_handler::CommandType::GET_REGISTER) {
      std::string reg_str = command.args[0];
      if (reg_str[0] == 'x') {
        std::cout << "VM_REGISTER_VAL_START";
        std::cout << "0x"
                  << std::hex
                  << vm->registers_.ReadGpr(std::stoi(reg_str.substr(1))) 
                  << std::dec;
        std::cout << "VM_REGISTER_VAL_END"<< std::endl;
      } 
    }

  
    else if (command.type==command_handler::CommandType::MODIFY_MEMORY) {
      if (command.args.size() != 3) {
        std::cout << "VM_MODIFY_MEMORY_ERROR" << std::endl;
        continue;
      }
      try {
        uint64_t address = std::stoull(command.args[0], nullptr, 16);
        std::string type = command.args[1];
        uint64_t value = std::stoull(command.args[2], nullptr, 16);

        if (type == "byte") {
          vm->memory_controller_.WriteByte(address, static_cast<uint8_t>(value));
        } else if (type == "half") {
          vm->memory_controller_.WriteHalfWord(address, static_cast<uint16_t>(value));
        } else if (type == "word") {
          vm->memory_controller_.WriteWord(address, static_cast<uint32_t>(value));
        } else if (type == "double") {
          vm->memory_controller_.WriteDoubleWord(address, value);
        } else {
          std::cout << "VM_MODIFY_MEMORY_ERROR" << std::endl;
          continue;
        }
        std::cout << "VM_MODIFY_MEMORY_SUCCESS" << std::endl;
      } catch (const std::out_of_range &e) {
        std::cout << "VM_MODIFY_MEMORY_ERROR" << std::endl;
        continue;
      } catch (const std::exception& e) {
        std::cout << "VM_MODIFY_MEMORY_ERROR" << std::endl;
        continue;
      }
    }
    
    
    
    else if (command.type==command_handler::CommandType::DUMP_MEMORY) {
      try {
  vm->memory_controller_.DumpMemory(command.args);
      } catch (const std::out_of_range &e) {
        std::cout << "VM_MEMORY_DUMP_ERROR" << std::endl;
        continue;
      } catch (const std::exception& e) {
        std::cout << "VM_MEMORY_DUMP_ERROR" << std::endl;
        continue;
      }
    } else if (command.type==command_handler::CommandType::PRINT_MEMORY) {
      for (size_t i = 0; i < command.args.size(); i+=2) {
        uint64_t address = std::stoull(command.args[i], nullptr, 16);
        uint64_t rows = std::stoull(command.args[i+1]);
  vm->memory_controller_.PrintMemory(address, rows);
      }
      std::cout << std::endl;
    } else if (command.type==command_handler::CommandType::GET_MEMORY_POINT) {
      if (command.args.size() != 1) {
        std::cout << "VM_GET_MEMORY_POINT_ERROR" << std::endl;
        continue;
      }
      // uint64_t address = std::stoull(command.args[0], nullptr, 16);
  vm->memory_controller_.GetMemoryPoint(command.args[0]);
    } 


    else if (command.type==command_handler::CommandType::VM_STDIN) {
  vm->PushInput(command.args[0]);
    }
    
    
    else if (command.type==command_handler::CommandType::DUMP_CACHE) {
      std::cout << "Cache dumped." << std::endl;
    } else {
      std::cout << "Invalid command.";
      std::cout << command_buffer << std::endl;
    }

  }






  return 0;
}