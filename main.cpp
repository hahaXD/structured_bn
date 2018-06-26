//
// Created by Yujia Shen on 4/12/18.
//
//
// Created by Yujia Shen on 3/20/18.
//
extern "C" {
#include <sdd/sddapi.h>
}
#include <iostream>
#include <structured_bn/network.h>
#include <util/optionparser.h>
#include <cassert>
#include <chrono>
#include <structured_bn/network_compiler.h>
using ms = std::chrono::milliseconds;
using get_time = std::chrono::steady_clock;

struct Arg : public option::Arg {
  static void printError(const char *msg1, const option::Option &opt, const char *msg2) {
    fprintf(stderr, "%s", msg1);
    fwrite(opt.name, (size_t) opt.namelen, 1, stderr);
    fprintf(stderr, "%s", msg2);
  }

  static option::ArgStatus Required(const option::Option &option, bool msg) {
    if (option.arg != 0)
      return option::ARG_OK;

    if (msg) printError("Option '", option, "' requires an argument\n");
    return option::ARG_ILLEGAL;
  }

  static option::ArgStatus Numeric(const option::Option &option, bool msg) {
    char *endptr = 0;
    if (option.arg != 0 && strtol(option.arg, &endptr, 10)) {};
    if (endptr != option.arg && *endptr == 0)
      return option::ARG_OK;

    if (msg) printError("Option '", option, "' requires a numeric argument\n");
    return option::ARG_ILLEGAL;
  }
};
enum optionIndex {
  UNKNOWN,
  HELP,
  SPARSE_LEARNING_DATASET_FILE,
  LEARNING_DATASET_FILE,
  PSDD_FILENAME,
  VTREE_FILENAME,
  CONSISTENT_CHECK,
  SAMPLE_PARAMETER,
  SEED
};

const option::Descriptor usage[] =
    {
        {UNKNOWN, 0, "", "", option::Arg::None, "USAGE: example [options]\n\n \tOptions:"},
        {HELP, 0, "h", "help", option::Arg::None, "--help  \tPrint usage and exit."},
        {SPARSE_LEARNING_DATASET_FILE, 0, "", "sparse_learning_dataset", Arg::Required,
         "--sparse_learning_dataset Set sparse dataset file which is used to learn parameters in the SBN\""},
        {LEARNING_DATASET_FILE, 0, "", "learning_dataset", Arg::Required,
         "--learning_dataset Set dataset file which is used to learn parameters in the SBN"},
        {PSDD_FILENAME, 0, "", "psdd_filename", Arg::Required,
         "--psdd_filename the output filename for the compiled psdd."},
        {VTREE_FILENAME, 0, "", "vtree_filename", Arg::Required,
         "--vtree_filename the output filename for joint vtree"},
        {CONSISTENT_CHECK, 0, "", "consistent_check", option::Arg::None,
         "--consistent_check \tCheck whether learning data is consistent"},
        {SAMPLE_PARAMETER, 0, "", "sample_parameter", option::Arg::None,
         "--sample_parameter \t Sample parameter from Gamma distribution"},
        {SEED, 0, "s", "seed", Arg::Required, "--seed \t Seed to be used. default is 0"},
        {UNKNOWN, 0, "", "", option::Arg::None,
         "\nExamples:\n./structured_bn_main --psdd_filename <psdd_filename> --vtree_filename <vtree_filename> network.json\n"
        },
        {0, 0, 0, 0, 0, 0}
    };

using structured_bn::Network;
using structured_bn::NetworkCompiler;
int main(int argc, const char *argv[]) {
  argc -= (argc > 0);
  argv += (argc > 0); // skip program name argv[0] if present
  option::Stats stats(usage, argc, argv);
  std::vector<option::Option> options(stats.options_max);
  std::vector<option::Option> buffer(stats.buffer_max);
  option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);
  if (parse.error())
    return 1;
  if (options[HELP] || argc == 0) {
    option::printUsage(std::cout, usage);
    return 0;
  }
  const char *network_file = parse.nonOption(0);
  uint seed = 0;
  if (options[SEED]) {
    seed = (uint) std::strtol(options[SEED].arg, nullptr, 10);
  }
  std::cout << "Loading Network File " << network_file << std::endl;
  auto start = get_time::now();
  Network *network = Network::GetNetworkFromSpecFile(network_file);
  auto end = get_time::now();
  std::cout << "Network Loading Time : " << std::chrono::duration_cast<ms>(end - start).count() << " ms" << std::endl;
  if (options[SAMPLE_PARAMETER]) {
    std::cout << "Sample parameters in the network" << std::endl;
    RandomDoubleFromGammaGenerator generator(1.0, 1.0, seed);
    network->SampleParameters(&generator);
  } else {
    BinaryData *train_data = nullptr;
    if (options[LEARNING_DATASET_FILE]) {
      const char *data_file = options[LEARNING_DATASET_FILE].arg;
      std::cout << "Learning parameters from data file " << data_file << std::endl;
      train_data = new BinaryData();
      train_data->ReadFile(data_file);

    } else if (options[SPARSE_LEARNING_DATASET_FILE]) {
      const char *data_file = options[SPARSE_LEARNING_DATASET_FILE].arg;
      std::cout << "Learning parameter from sparse data file " << data_file << std::endl;
      train_data = BinaryData::ReadSparseDataJsonFile(data_file);
    } else {
      std::cout << "Learning parameter with 0 data" << std::endl;
      train_data = new BinaryData();
    }
    start = get_time::now();
    network->LearnParametersUsingLaplacianSmoothing(train_data, PsddParameter::CreateFromDecimal(1));
    end = get_time::now();
    std::cout << "Learn Parameter Time : " << std::chrono::duration_cast<ms>(end - start).count() << " ms" << std::endl;
    if (options[CONSISTENT_CHECK]) {
      assert(train_data != nullptr);
      const auto &dataset = train_data->data();
      std::bitset<MAX_VAR> mask;
      mask.set();
      for (const auto &cur_entry : dataset) {
        std::cout << "Data : " << cur_entry.first << std::endl;
        if (network->IsModel(mask, cur_entry.first)) {
          std::cout << "is a Model" << std::endl;
        } else {
          std::cout << "is not a Model" << std::endl;
        }
      }
    }
    delete (train_data);
  }
  if (options[PSDD_FILENAME]) {
    const char *psdd_filename = options[PSDD_FILENAME].arg;
    start = get_time::now();
    NetworkCompiler *compiler = NetworkCompiler::GetDefaultNetworkCompiler(network);
    auto result = compiler->Compile();
    end = get_time::now();
    std::cout << "Compile Network Time : " << std::chrono::duration_cast<ms>(end - start).count() << " ms" << std::endl;
    auto model_count = psdd_node_util::ModelCount(psdd_node_util::SerializePsddNodes(result.first));
    std::cout << "Model count " << model_count.get_str(10) << std::endl;
    std::cout << "PSDD size" << psdd_node_util::GetPsddSize(result.first) << std::endl;
    psdd_node_util::WritePsddToFile(result.first, psdd_filename);
    if (options[VTREE_FILENAME]) {
      const char *vtree_filename = options[VTREE_FILENAME].arg;
      sdd_vtree_save(vtree_filename, compiler->GetVtree());
    }
    delete (compiler);
  }
  delete (network);
}
