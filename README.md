# Structured Bayesian Networks 

This project aims to compile a structured Bayesian network that is described by a json file to a single PSDD. For example, medium\_sf\_sdd.json is an example of a json file that describes an SBN. Further, the direcotry medium\_sf\_sdds  contains many sdd files that are referenced from the json file. 

## Compile

```bash
mkdir build
cd build
cmake3 ..
make
```
After running the commands above, two binaries should be generated, structured\_bn\_main and structured\_bn\_test. To test the setup, please run 

```bash
./structured_bn_test
```

You should see all Passes.


## Run 

```bash
./structured_bn_main --learning_dataset=<path_to_dataset_file> --psdd_filename <output_psdd_filename> --vtree_filename <output_vtree_filename> <sbn_json_file> 
```

This will first learn weights of a SBN structure that is specified by <sbn\_json\_file>, and compiled the learned SBN into a single PSDD. The compiled PSDD is stored in output\_psdd\_filename and the used vtree is also stored in output\_vtree\_filename.

For example, the following command compiles an SBN whose structure is specified by medium\_sf\_sdd.json. The weights of the SBN is randomly sampled, instead of being learned from a dataset.

```bash
./structured_bn_main --sample_parameter --psdd_filename=medium_sf_sdd.psdd --vtree_filename=medium_sf_sdd.vtree  medium_sf_sdd.json
```

## Dataset
The learning dataset is a comma separated csv, where the column i represents the value of variable i+1. The index of a variable starts from 1.

For example, the following datafile specifies a single training example where the variable 1 is 1 and variable 2 is 0.

```
1 0
```

