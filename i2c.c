#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define 	MISSING_FUNC_FMT   "Error: Adapter does not have %s capability\n"
#define 	END_OPT 			-1
#define 	TRUE 				1
#define		FALSE				0

static const char *optString = "r:s:g:h?";
struct globalArgs_t
{
	char* reg;
	char* set;
	char* get;
	char help;
} globalArgs;

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
	return -1;
	}
	return file; 
}

static inline  int set_slave_addr(int file, int address)
{
	/* With force, let the user read from/write to the registers
	even when a driver is also running */
	if (ioctl(file, I2C_SLAVE, address) < 0) {
		fprintf(stderr,
				"Error: Could not set address to 0x%02x: %s\n",
			address, strerror(errno));
		return -errno;
	}

    return 0;
}
static inline int check_funcs(int file, int daddress)
{
	unsigned long funcs;

	/* check adapter functionality */
	if (ioctl(file, I2C_FUNCS, &funcs) < 0) {
	fprintf(stderr, "Error: Could not get the adapter "
	        "functionality matrix: %s\n", strerror(errno));
	return -1;
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
	return 0;
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
		return -1;

	else {
		for (i = 1; i <= data.block[0]; i++)
			values[i-1] = data.block[i];
	return data.block[0];
	}
}

void parse_args(int argc, char* argv[])
{
	int opt = 0;
	globalArgs.help = TRUE;
	globalArgs.get = 0;
	globalArgs.set = 0;

	opt = getopt(argc, argv, optString);
	while (opt != END_OPT)
	{
		switch (opt)
		{
		case 'r':
		{
			globalArgs.reg = optarg;
			globalArgs.help = FALSE;
			break;
		}
		case 's':
		{

			globalArgs.set = optarg;
			globalArgs.help = FALSE;
			break;
		}
		case 'g':
		{
			globalArgs.get = optarg;
			globalArgs.help = FALSE;
			break;
		}
		default:
		{
			break;
		}
		}
		opt = getopt(argc, argv, optString);
	}

}

int main(int argc,char* argv[])
{
	parse_args(argc,argv);
	char *end_ptr;
	if (globalArgs.help) {printf("%s\n","-r -s -g"); return 0;}
	int reg = strtol(globalArgs.reg,&end_ptr,16);
	if (errno == ERANGE || *end_ptr)
		{
			printf("Register is invalid");
			return -1;
		}
	if (!globalArgs.get && !globalArgs.set) 
	{
		printf("Set or Get ? \n");
		printf("   -s [value] set value\n");
		printf("   -g [size]  get size bytes \n");
		return -1;
	}
	printf("Register = %d\n",reg);
	char filename[20];
	int fd;

	// if ((fd = open_i2c_dev(2,filename,sizeof(filename),0))< 0) return -1; 
	// if (check_funcs(fd,0x48) < 0) return -1; 
	// if (set_slave_addr(fd,0x48) < 0) return -1;

	if (globalArgs.get) 
	{
		int size = strtol(globalArgs.get,&end_ptr,10);
		if (errno == ERANGE || *end_ptr)
		{
			printf("Size is invalid");
			return -1;
		}

		printf("Size = %d\n",size);
		int i = 0;
		__u8 block[size];
		int res = 0;
		res = i2c_smbus_read_i2c_block_data(fd,reg,size,block+0);
		printf("res = %d\n", res);
		printf("0x");
		for (;i<size;i++)
			printf("%02x", block[i]);
	}
	else
	{

	}

	printf("\n");
	close(fd);
	return 0;
}