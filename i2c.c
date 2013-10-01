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
struct globalArgs_t
{
	char reg;
	char* set;
	int get;
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
	return EXIT_NOT_SUCCESS;
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

    return EXIT_SUCCESS;
}
static inline int check_funcs(int file, int daddress)
{
	unsigned long funcs;

	/* check adapter functionality */
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

char parse_args(int argc, char* argv[])
{
	int opt = 0;
	globalArgs.help = TRUE;
	globalArgs.get = 0;
	globalArgs.set = 0;
	globalArgs.reg = 0;

	opt = getopt(argc, argv, optString);
	while (opt != END_OPT)
	{
		switch (opt)
		{
		case 'r':
		{
			if (sscanf(optarg, "0x%2hhx", &globalArgs.reg) != 1)
			{
				printf("Register is invalid\n");
				return EXIT_NOT_SUCCESS;
			}
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
		if (sscanf(optarg, "%d", &globalArgs.get) != 1) 
		{
			printf("Operand is invalid\n");
			return EXIT_NOT_SUCCESS;
		}
		if (globalArgs.get == 0)
		{
			printf("Operand is invalid\n");
			return EXIT_NOT_SUCCESS;
		}
			globalArgs.help = FALSE;
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
	if (globalArgs.get == 0 && globalArgs.set == 0) 
	{	
		printf("Set or Get ? \n");
		printf("   -s [value] set value\n");
		printf("   -g [size]  get size bytes \n");
		return EXIT_NOT_SUCCESS;
	}
	if (!globalArgs.reg)
	{
		printf("use -r [REGISTER] for set it\n");
		return EXIT_NOT_SUCCESS;
	}
	return EXIT_SUCCESS;
}

int main(int argc,char* argv[])
{
	if ((parse_args(argc,argv)) == 1) return EXIT_NOT_SUCCESS; 
	
	if (globalArgs.help) {printf("%s\n","-r -s -g"); return EXIT_SUCCESS;}
		
	
	char filename[20];
	int fd;

	if ((fd = open_i2c_dev(2,filename,sizeof(filename),0))< 0) return EXIT_NOT_SUCCESS; 
	if (check_funcs(fd,0x48) < 0) return EXIT_NOT_SUCCESS; 
	if (set_slave_addr(fd,0x48) < 0) return EXIT_NOT_SUCCESS;

	printf("Register = %x\n",globalArgs.reg);
	int i = 0;
	int res = 0;
	if (globalArgs.get) 
	{
		printf("get = %d\n", globalArgs.get);
		__u8 block[globalArgs.get];
		res = i2c_smbus_read_i2c_block_data(fd,globalArgs.reg,globalArgs.get,block+0);
		printf("res = %d\n", res);
		printf("0x");
		for (i = globalArgs.get-1;i>=0;i--)
			printf("%02x", block[i]);
	}
	else
	{
		__u8 block[20];
		unsigned char byteval;
		char f = FALSE;
		int size = 0;
		while (globalArgs.set[i] && !f )
		{
			if (sscanf(globalArgs.set+2*i, "%2hhx", &byteval) != 1)
			{
				f = TRUE;
				size = i;

			}
			else
			{
				block[i] = byteval;
				printf("block[%d]: %x\n",i,block[i]);
				i++;
			}
		}
		printf("size of array = %d\n",i);
		res = i2c_smbus_write_i2c_block_data(fd,globalArgs.reg,size,block+0);
		printf("Result of operation = %d\n", res);
	}

	printf("\n");
	close(fd);
	return EXIT_SUCCESS;
}