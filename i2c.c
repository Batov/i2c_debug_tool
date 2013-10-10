#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define 	MISSING_FUNC_FMT   "Error: Adapter does not have %s capability\n"
#define 	END_OPT 			-1
#define 	TRUE 				1
#define		FALSE				0
#define  	EXIT_SUCCESS		0
#define		EXIT_NOT_SUCCESS	1

static const char *optString = "r:s:g:h?";
struct Args_t
{
	char reg;
	__u8 set[30];
	int setSize;
	int get;
	char help;
} Args;

static inline __s32 i2c_smbus_access(int file, char read_write, __u8 command, 
	int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;
	return ioctl(file,I2C_SMBUS,&args);
}

static inline int open_i2c_dev(int i2cbus, char *filename, size_t size, int quiet)
{
	int file;

	snprintf(filename, size, "/dev/i2c/%d", i2cbus);
	filename[size - 1] = '\0';
	file = open(filename, O_RDWR);

	if (file < 0 && (errno == ENOENT || errno == ENOTDIR)) 
	{
		sprintf(filename, "/dev/i2c-%d", i2cbus);
		file = open(filename, O_RDWR);
	}

	if (file < 0 && !quiet) 
	{
		if (errno == ENOENT) 
		{
			fprintf(stderr, "Error: Could not open file "
				"`/dev/i2c-%d' or `/dev/i2c/%d': %s\n",
				i2cbus, i2cbus, strerror(ENOENT));
		} 
		else 
		{
			fprintf(stderr, "Error: Could not open file "
				"`%s': %s\n", filename, strerror(errno));
			if (errno == EACCES)
				fprintf(stderr, "Run as root?\n");
		}
		return EXIT_NOT_SUCCESS;
	}
	return file; 
}

static inline  int set_slave_addr(int file, int address)
{
	if (ioctl(file, I2C_SLAVE, address) < 0) {
		fprintf(stderr,
			"Error: Could not set address to 0x%02x: %s\n",
			address, strerror(errno));
		return -errno;
	}

	return EXIT_SUCCESS;
}
static inline int check_funcs(int file, int daddress)
{
	unsigned long funcs;

	if (ioctl(file, I2C_FUNCS, &funcs) < 0) {
		fprintf(stderr, "Error: Could not get the adapter "
			"functionality matrix: %s\n", strerror(errno));
		return EXIT_NOT_SUCCESS;
	}
	if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE)) {
		fprintf(stderr, MISSING_FUNC_FMT, "SMBus receive byte");
	}
	if (daddress >= 0
		&& !(funcs & I2C_FUNC_SMBUS_WRITE_BYTE)) {
		fprintf(stderr, MISSING_FUNC_FMT, "SMBus send byte");
}
if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
	fprintf(stderr, MISSING_FUNC_FMT, "SMBus read byte");
}
if (!(funcs & I2C_FUNC_SMBUS_READ_WORD_DATA)) {
	fprintf(stderr, MISSING_FUNC_FMT, "SMBus read word");
}
if (!(funcs & I2C_FUNC_SMBUS_READ_BLOCK_DATA)) {
	fprintf(stderr, MISSING_FUNC_FMT, "SMBus read block data");
}
if (!(funcs & I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
	fprintf(stderr, MISSING_FUNC_FMT, "SMBus read i2c block data");
}
return EXIT_SUCCESS;
}


static inline  __s32 i2c_smbus_read_i2c_block_data	(	int 	file,
	__u8 	command,
	__u8 	length,
	__u8 * 	values 
	){
	union i2c_smbus_data data;
	int i;

	if (length > 32)
		length = 32;

	data.block[0] = length;
	if (i2c_smbus_access(file,I2C_SMBUS_READ,command,
		length == 32 ? I2C_SMBUS_I2C_BLOCK_BROKEN :
		I2C_SMBUS_I2C_BLOCK_DATA,&data))
		return EXIT_NOT_SUCCESS;

	else {
		for (i = 1; i <= data.block[0]; i++)
			values[i-1] = data.block[i];
		return data.block[0];
	}
}

static inline __s32 i2c_smbus_write_i2c_block_data(int file, __u8 command,
	__u8 length, __u8 *values)
{
	union i2c_smbus_data data;
	int i;
	if (length > 32)
		length = 32;	
	for (i = 1; i <= length; i++)
		data.block[i] = values[i-1];
	data.block[0] = length;
	return i2c_smbus_access(file,I2C_SMBUS_WRITE,command,
		I2C_SMBUS_I2C_BLOCK_DATA, &data);
}
static inline void init_args()
{
	Args.help = TRUE;
	Args.get = 0;
	Args.reg = 0;
	Args.setSize = 0;

}
static inline void reverse()
{
	int i;
	__u8 swap;
	for (i = 0; i < Args.setSize/2; ++i)
	{
		swap = Args.set[i];
		Args.set[i] = Args.set[Args.setSize-1-i];
		Args.set[Args.setSize-1-i] = swap;
	}
}

static inline int parse_setArg()
{
	int i = 0;
	unsigned char byteval;
	char f = FALSE;
	if (optarg[0] != '0' && optarg[1] != 'x') 
	{
		printf("Use 0x0 format\n");
		return EXIT_NOT_SUCCESS;
	}
	else optarg += 2;
	while (optarg[i] && !f)
	{
		if (sscanf(optarg+2*i, "%2hhx", &byteval) != 1)
		{
			f = TRUE;
			Args.setSize = i;
		}	
		else
		{	
			if (i == 0) Args.setSize = 1;   
			Args.set[i] = byteval;
			i++;
		}
		reverse();
	}
	return EXIT_SUCCESS;
}

static inline int parse_getArg()
{
	if (sscanf(optarg, "%d", &Args.get) != 1) 
	{
		printf("Operand is invalid\n");
		return EXIT_NOT_SUCCESS;
	}
	if (Args.get == 0)
	{
		printf("Operand is invalid\n");
		return EXIT_NOT_SUCCESS;
	}
	return EXIT_SUCCESS;
}

static inline char parse_args(int argc, char* argv[])
{
	init_args();
	int opt = getopt(argc, argv, optString);
	while (opt != END_OPT)
	{
		switch (opt)
		{
			case 'r':
			{
				if (sscanf(optarg, "0x%2hhx", &Args.reg) != 1)
				{
					printf("Register is invalid\n");
					printf("Use 0x0 format\n");
					return EXIT_NOT_SUCCESS;
				}
				Args.help = FALSE;
				break;
			}
			case 's':
			{
				if (Args.get != 0) // if set and get together
				{
					printf("set or get?\n");
					return EXIT_NOT_SUCCESS;
				}
				if (parse_setArg() != EXIT_SUCCESS) return EXIT_NOT_SUCCESS;
				Args.help = FALSE;
				break;
			}
			case 'g':
			{
				if (Args.setSize != 0) // if set and get together
				{
					printf("set or get?\n");
					return EXIT_NOT_SUCCESS;
				}
				if (parse_getArg() != EXIT_SUCCESS) return EXIT_NOT_SUCCESS;
				Args.help = FALSE;
				break;
			}
			default:
			{
				return EXIT_NOT_SUCCESS;
				break;
			}
		}
		opt = getopt(argc, argv, optString);
	}
	if ((Args.setSize == 0) && (Args.get == 0))
	{
		printf("set or get?\n");
		Args.help = TRUE;    	
	}

	if ((Args.help)) 
	{	
		printf("I2C tool \n");
		printf("   -s [VALUE] set value to slave\n");
		printf("   -g [SIZE]  get bytes from slave\n");
		printf("   -r [REGISTER]\n");
		printf("e.g.  i2c -s 0x64 -r 0x16\n");
		printf("      i2c -g 4 -r 0x26\n");
		return EXIT_NOT_SUCCESS;
	}
	if (!Args.reg)
	{
		printf("   -r [REGISTER]\n");
		return EXIT_NOT_SUCCESS;
	}
	return EXIT_SUCCESS;
}

int main(int argc,char* argv[])
{
	if ((parse_args(argc,argv)) == 1) return EXIT_NOT_SUCCESS; 
	if (Args.help == EXIT_NOT_SUCCESS) return EXIT_NOT_SUCCESS;
	char filename[20];
	int fd;

	if ((fd = open_i2c_dev(2,filename,sizeof(filename),0))< 0) return EXIT_NOT_SUCCESS; 
	if (check_funcs(fd,0x48) < 0) return EXIT_NOT_SUCCESS; 
	if (set_slave_addr(fd,0x48) < 0) return EXIT_NOT_SUCCESS;

	int i = 0;
	if (Args.get) 
	{
		__u8 block[Args.get];
		if  (i2c_smbus_read_i2c_block_data(fd,Args.reg,Args.get,block+0) != Args.get) 
		{
			printf("Get data failed");
			return EXIT_NOT_SUCCESS;
		}
		else 
		{
			printf("0x");
			for (i = Args.get-1;i>=0;i--)
				printf("%02x", block[i]);
			printf("\n");
		}
	} 
	else
	{
		if (i2c_smbus_write_i2c_block_data(fd,Args.reg,Args.setSize,Args.set) != EXIT_SUCCESS)
		{
			printf("Set data failed");
			return EXIT_NOT_SUCCESS;
		}
		
	}
	close(fd);
	return EXIT_SUCCESS;
}