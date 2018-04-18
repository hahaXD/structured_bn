//
// Created by Yujia Shen on 4/12/18.
//
//
// Created by Yujia Shen on 3/20/18.
//
extern "C" {
#include <sddapi.h>
}
#include <iostream>
#include <network.h>
#include "optionparser.h"
#include "network_compiler.h"

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
  DATASET_FILE,
  PSDD_FILENAME,
  VTREE_FILENAME,
  CNF_EVID
};

const option::Descriptor usage[] =
    {
        {UNKNOWN, 0, "", "", option::Arg::None, "USAGE: example [options]\n\n \tOptions:"},
        {HELP, 0, "h", "help", option::Arg::None, "--help  \tPrint usage and exit."},
        {DATASET_FILE, 0, "", "learning_dataset", Arg::Required,
         "--learning_dataset Set dataset file which is used to learn parameters in the SBN"},
        {CNF_EVID, 0, "", "cnf_evid", Arg::Required, "--cnf_evid  evid file, represented using CNF."},
        {PSDD_FILENAME, 0, "", "psdd_filename", Arg::Required,
         "--psdd_filename the output filename for the compiled psdd."},
        {VTREE_FILENAME, 0, "", "vtree_filename", Arg::Required,
         "--vtree_filename the output filename for joint vtree"},
        {UNKNOWN, 0, "", "", option::Arg::None,
         "\nExamples:\n./sbn_main --sample_parameter  --mpe_query network.json "},
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
  Network *network = Network::GetNetworkFromSpecFile(network_file);
  BinaryData *train_data = nullptr;
  if (options[DATASET_FILE]) {
    const char *data_file = options[DATASET_FILE].arg;
    train_data = new BinaryData();
    train_data->ReadFile(data_file);

  } else {
    train_data = new BinaryData();
  }
  network->LearnParametersUsingLaplacianSmoothing(train_data, PsddParameter::CreateFromDecimal(1));
  delete (train_data);
  if (options[PSDD_FILENAME]) {
    const char *psdd_filename = options[PSDD_FILENAME].arg;
    NetworkCompiler *compiler = NetworkCompiler::GetDefaultNetworkCompiler(network);
    auto result = compiler->Compile();
    auto model_count = psdd_node_util::ModelCount(psdd_node_util::SerializePsddNodes(result.first));
    std::cout << "Model count " << model_count.get_str(10) << std::endl;
    psdd_node_util::WritePsddToFile(result.first, psdd_filename);
    if (options[VTREE_FILENAME]){
      const char* vtree_filename = options[VTREE_FILENAME].arg;
      sdd_vtree_save(vtree_filename, compiler->GetVtree());
    }
    delete (compiler);
  }
  delete (network);
  delete (train_data);
}
