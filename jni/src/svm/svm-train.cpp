#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "svm.h"
#include "svm-train.h"
#include "../log.h"
#define Malloc(type,n) (type *)malloc((n)*sizeof(type))
#define LOG_TAG "TRAIN"

void print_null(const char *s) {}

void exit_with_help_for_train()
{
	LOGD("Designated options are incorrect.\
                Please refer to http://ntu.csie.org/~piaip/svm/svm_tutorial.html");
	exit(1);
}

void exit_input_error(int line_num)
{
	LOGD("Wrong input format at line %d\n", line_num);
	exit(1);
}

void parse_command_line(int argc, char **argv, char *train_file_name, char *model_file_name);
void parse_command_line(int argc, char **argv, double **arrayXtr, char *model_file_name);
void read_problem(const char *filename);
void read_problem(double **arrayXtr, int *arrayYtr, int **arrayIdx, int nRow, int *arrayNcol);
void do_cross_validation();

struct svm_parameter param;		// set by parse_command_line
struct svm_problem prob;		// set by read_problem
struct svm_model *modelt;               // model for train
struct svm_node *x_space;
int cross_validation;
int nr_fold;

static char *line = NULL;
static int max_line_len;

static char* readline(FILE *input)
{
	int len;

	if(fgets(line,max_line_len, input) == NULL)
		return NULL;

	while(strrchr(line,'\n') == NULL)
	{
		max_line_len *= 2;
		line = (char *) realloc(line, max_line_len);
		len = (int) strlen(line);
		if(fgets(line+len,max_line_len-len,input) == NULL)
			break;
	}
	return line;
}

void setParam(int argc, char** argv) {
	int i;
	void (*print_func)(const char*) = NULL;	// default printing to stdout

	// default values
	param.svm_type = C_SVC;
	param.kernel_type = RBF;
	param.degree = 3;
	param.gamma = 0;	// 1/num_features
	param.coef0 = 0;
	param.nu = 0.5;
	param.cache_size = 100;
	param.C = 1;
	param.eps = 1e-3;
	param.p = 0.1;
	param.shrinking = 1;
	param.probability = 0;
	param.nr_weight = 0;
	param.weight_label = NULL;
	param.weight = NULL;
	cross_validation = 0;

	// parse options
	for(i=1;i<argc;i++)
	{
		if(argv[i][0] != '-') break;

		if(++i>=argc)
			exit_with_help_for_train();

		switch(argv[i-1][1])
		{
		case 's':
			param.svm_type = atoi(argv[i]);
			break;
		case 't':
			param.kernel_type = atoi(argv[i]);
			break;
		case 'd':
			param.degree = atoi(argv[i]);
			break;
		case 'g':
			param.gamma = atof(argv[i]);
			break;
		case 'r':
			param.coef0 = atof(argv[i]);
			break;
		case 'n':
			param.nu = atof(argv[i]);
			break;
		case 'm':
			param.cache_size = atof(argv[i]);
			break;
		case 'c':
			param.C = atof(argv[i]);
			break;
		case 'e':
			param.eps = atof(argv[i]);
			break;
		case 'p':
			param.p = atof(argv[i]);
			break;
		case 'h':
			param.shrinking = atoi(argv[i]);
			break;
		case 'b':
			param.probability = atoi(argv[i]);
			break;
		case 'q':
			print_func = &print_null;
			i--;
			break;
		case 'v':
			cross_validation = 1;
			nr_fold = atoi(argv[i]);
			if(nr_fold < 2)
			{
				LOGD("n-fold cross validation: n must >= 2\n");
				exit_with_help_for_train();
			}
			break;
		case 'w':
			++param.nr_weight;
			param.weight_label = (int *)realloc(param.weight_label,sizeof(int)*param.nr_weight);
			param.weight = (double *)realloc(param.weight,sizeof(double)*param.nr_weight);
			param.weight_label[param.nr_weight-1] = atoi(&argv[i-1][2]);
			param.weight[param.nr_weight-1] = atof(argv[i]);
			break;
		default:
			LOGD("Unknown option: -%c\n", argv[i-1][1]);
			exit_with_help_for_train();
		}
	}

	svm_set_print_string_function(print_func);
}

/*
 * argc is the array size of parameter array
 * argv is a string array, containing parameters and the file paths
 *
 *
 * Be sure to put the parameters first, then train data, followed by model file
 */
int svmtrain(int argc, char **argv)
{
	char train_file_name[1024];
	char model_file_name[1024];
	const char *error_msg;

	/* Set parameters */
	setParam(argc, argv);

	/*
	 * Determine filenames, copy fro
	 * m the arguments to string/char array
	 * note: we put them at the rear of arguments
	 */
	// training data
	strcpy(train_file_name, argv[argc-2]);
	// model output
	strcpy(model_file_name, argv[argc-1]);

	/* Load the input into svm_prob format*/
	read_problem(train_file_name);

	/* Check before use it */
	error_msg = svm_check_parameter(&prob, &param);

	if(error_msg)
	{
		LOGD("ERROR: %s\n",error_msg);
		exit(1);
	}

	if(cross_validation)
	{
		do_cross_validation();
	}
	else
	{
		LOGD("train file %s\n", train_file_name);
		LOGD("model file %s\n", model_file_name);
		modelt = svm_train(&prob, &param);
		if(svm_save_model(model_file_name, modelt))
		{
			LOGD("can't save model to file %s\n", model_file_name);
			exit(1);
		}
		svm_free_and_destroy_model(&modelt);
	}
	svm_destroy_param(&param);
	free(prob.y);
	free(prob.x);
	free(x_space);
	free(line);

	return 0;
}

/*
 * argc is the array size of parameter array
 * argv is a string array, containing parameters and the file paths
 * arrayXtr is a 2-D double array for training data feature
 * arrayYtr is a 1-D int array for training data label
 *
 * Be sure to put the parameters first, then the model file
 */
int svmtrain(int argc, char **argv, double **arrayXtr, int *arrayYtr, int **arrayIdxTr, int nRow, int *arrayNcol)
{
	char model_file_name[1024];
	const char *error_msg;

	/* Set parameters */
	setParam(argc, argv);
	/* Determine the model file path */
	strcpy(model_file_name, argv[argc-1]);

	/* Load the input into svm_prob format*/
	read_problem(arrayXtr, arrayYtr, arrayIdxTr, nRow, arrayNcol);

	/* Check before use it */
	error_msg = svm_check_parameter(&prob, &param);

	if(error_msg)
	{
		LOGD("ERROR: %s\n",error_msg);
		exit(1);
	}

	if(cross_validation)
	{
		do_cross_validation();
	}
	else
	{
		modelt = svm_train(&prob,&param);
		if(svm_save_model(model_file_name,modelt))
		{
			LOGD("can't save model to file %s\n", model_file_name);
			exit(1);
		}
		svm_free_and_destroy_model(&modelt);
	}
	svm_destroy_param(&param);
	free(prob.y);
	free(prob.x);
	free(x_space);
	return 0;
}

void do_cross_validation()
{
	int i;
	int total_correct = 0;
	double total_error = 0;
	double sumv = 0, sumy = 0, sumvv = 0, sumyy = 0, sumvy = 0;
	double *target = Malloc(double,prob.l);

	svm_cross_validation(&prob,&param,nr_fold,target);
	if(param.svm_type == EPSILON_SVR ||
			param.svm_type == NU_SVR)
	{
		for(i=0;i<prob.l;i++)
		{
			double y = prob.y[i];
			double v = target[i];
			total_error += (v-y)*(v-y);
			sumv += v;
			sumy += y;
			sumvv += v*v;
			sumyy += y*y;
			sumvy += v*y;
		}
		LOGD("Cross Validation Mean squared error = %g\n", total_error/prob.l);
		LOGD("Cross Validation Squared correlation coefficient = %g\n",
				((prob.l*sumvy-sumv*sumy)*(prob.l*sumvy-sumv*sumy))/
				((prob.l*sumvv-sumv*sumv)*(prob.l*sumyy-sumy*sumy))
		);
	}
	else
	{
		for(i=0;i<prob.l;i++)
			if(target[i] == prob.y[i])
				++total_correct;
		LOGD("Cross Validation Accuracy = %g%%\n", 100.0*total_correct/prob.l);
	}
	free(target);
}

// read in a problem (in svmlight format)
void read_problem(const char *filename)
{
	int elements, max_index, inst_max_index, i, j;
	LOGD("About to open file %s", filename);
	FILE *fp = fopen(filename,"r");
	char *endptr;
	char *idx, *val, *label;

	if(fp == NULL)
	{
		LOGD("can't open input file %s\n",filename);
		exit(1);
	}

	prob.l = 0;
	elements = 0;

	max_line_len = 1024;
	line = Malloc(char,max_line_len);
	while(readline(fp)!=NULL)
	{
		char *p = strtok(line," \t"); // label

		// features
		while(1)
		{
			p = strtok(NULL," \t");
			if(p == NULL || *p == '\n') // check '\n' as ' ' may be after the last feature
				break;
			++elements;
		}
		// spare one more space for separating the records
		++elements;
		// the number of records
		++prob.l;
	}
	rewind(fp);

//	LOGD("nElements:%d\n", elements);
//	LOGD("numRow:%d\n", prob.l);

	prob.y = Malloc(double,prob.l);
	prob.x = Malloc(struct svm_node *,prob.l);
	x_space = Malloc(struct svm_node,elements);

	max_index = 0;
	j=0;
	for(i=0;i<prob.l;i++)
	{
		inst_max_index = -1; // strtol gives 0 if wrong format, and precomputed kernel has <index> start from 0
		readline(fp);
		prob.x[i] = &x_space[j];
		label = strtok(line," \t\n");
		if(label == NULL) // empty line
			exit_input_error(i+1);

		prob.y[i] = strtod(label,&endptr);
		if(endptr == label || *endptr != '\0')
			exit_input_error(i+1);

		while(1)
		{
			idx = strtok(NULL,":");
			val = strtok(NULL," \t");

			if(val == NULL)
				break;

			errno = 0;
			x_space[j].index = (int) strtol(idx,&endptr,10);
			if(endptr == idx || errno != 0 || *endptr != '\0' || x_space[j].index <= inst_max_index)
				exit_input_error(i+1);
			else
				inst_max_index = x_space[j].index;

			errno = 0;
			x_space[j].value = strtod(val,&endptr);
			if(endptr == val || errno != 0 || (*endptr != '\0' && !isspace(*endptr)))
				exit_input_error(i+1);

			++j;
		}

		if(inst_max_index > max_index)
			max_index = inst_max_index;
		// add a index -1 to separate the rows
		x_space[j++].index = -1;
	}

	if(param.gamma == 0 && max_index > 0)
		param.gamma = 1.0/max_index;

	if(param.kernel_type == PRECOMPUTED)
		for(i=0;i<prob.l;i++)
		{
			if (prob.x[i][0].index != 0)
			{
				LOGD("Wrong input format: first column must be 0:sample_serial_number\n");
				exit(1);
			}
			if ((int)prob.x[i][0].value <= 0 || (int)prob.x[i][0].value > max_index)
			{
				LOGD("Wrong input format: sample_serial_number out of range\n");
				exit(1);
			}
		}

	fclose(fp);
}

// read in a problem (in arrays)
void read_problem(double **arrayXtr, int *arrayYtr, int **arrayIdx, int nRow, int *arrayNcol)
{
	// Get the total number of items in the svm x_space
	int nElements = 0;
	for(int row = 0; row < nRow; row++) {
		// for the feature items
		nElements += arrayNcol[row];
		// for separating the records
		++nElements;
	}

	prob.l = nRow;

//	LOGD("nElements:%d\n", nElements);
//	LOGD("numRow:%d\n", nRow);
	prob.y = Malloc(double,prob.l);
	prob.x = Malloc(struct svm_node *, prob.l);
	x_space = Malloc(struct svm_node, nElements);

	int max_index = 0;
	int idxElement = 0;
	int nCol;
	for(int row = 0; row < prob.l; row++)
	{
		prob.x[row] = &x_space[idxElement];
		prob.y[row] = arrayYtr[row];
		nCol = arrayNcol[row];

		for(int col = 0; col < nCol; col++)
		{
			x_space[idxElement].index = arrayIdx[row][col];
			x_space[idxElement].value = arrayXtr[row][col];
			++idxElement;
		}

		if(nCol > max_index)
			max_index = nCol;
		// use index -1 to separate the records
		x_space[idxElement++].index = -1;
	}

	if(param.gamma == 0 && max_index > 0)
		param.gamma = 1.0/max_index;

	if(param.kernel_type == PRECOMPUTED)
		for(int row = 0; row < prob.l; row++)
		{
			if (prob.x[row][0].index != 0)
			{
				LOGD("Wrong input format: first column must be 0:sample_serial_number\n");
				exit(1);
			}
			if ((int)prob.x[row][0].value <= 0 || (int)prob.x[row][0].value > max_index)
			{
				LOGD("Wrong input format: sample_serial_number out of range\n");
				exit(1);
			}
		}
}
