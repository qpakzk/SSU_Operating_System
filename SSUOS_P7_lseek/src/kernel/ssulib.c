#include <ssulib.h>
#include <device/console.h>
#include <device/io.h>
#include <syscall.h>
#include <filesys/file.h>
#include <string.h>

//option flag
#define NO_OPT 0x0
#define E_OPT  0x1
#define A_OPT  0x2
#define RE_OPT 0x4
#define C_OPT  0x8

// void memcpy(void* from, void* to, uint32_t len)
// {
// 	uint32_t *p1 = (uint32_t*)from;
// 	uint32_t *p2 = (uint32_t*)to;
// 	int i, e;

// 	e = len/sizeof(uint32_t);
// 	for(i = 0; i<e; i++)
// 		p2[i] = p1[i];

// 	e = len%sizeof(uint32_t);
// 	if( e != 0)
// 	{
// 		uint8_t *p3 = (uint8_t*)p1;
// 		uint8_t *p4 = (uint8_t*)p2;

// 		for(i = 0; i<e; i++)
// 			p4[i] = p3[i];
// 	}
// }

int strncmp(char* b1, char* b2, int len)
{
	int i;

	for(i = 0; i < len; i++)
	{
		char c = b1[i] - b2[i];
		if(c)
			return c;
		if(b1[i] == 0)
			return 0;
	}
	return 0;
}

bool getkbd(char *buf, int len) 
{
	char ch;
	int offset = 0;

	len--;

	for(; offset < len && buf[offset] != 0; offset++)
		if(buf[offset] == '\n')
		{
			for(;offset>=0;offset--)
				buf[offset] = 0;
			offset++;
			break;
		}

	while ( (ch = ssuread()) >= 0)
	{
		if(ch == '\b' && offset == 0)
		{
			set_cursor();
			continue;
		}
		printk("%c",ch);
		set_cursor();
		if (ch == '\b')
		{
			buf[offset-1] = 0;
			offset--;
		}
		else if (ch == '\n')
		{
			buf[offset] = ch;
			return FALSE;
		}
		else
		{
			buf[offset] = ch;
			offset++;
		}
		if(offset == len) offset--;
	}
	
	/*
	{
		if (ch == '\b')
		{
			if(offset == 0)
			{
				set_cursor();
				continue;
			}
			buf[offset-1] = 0;
			offset -= 2;
			printk("%c",ch);
		}
		else if (ch == '\n')
		{
			buf[offset] = ch;
			printk("%c",ch);
			return FALSE;
		}
		else
		{
			buf[offset] = ch;
			printk("%c",ch);
		}

		if(offset < len) offset++;
	}*/

	return TRUE;
}


int getToken(char* buf, char token[][BUFSIZ], int max_tok)
{
	int num_tok = 0;
	int off_tok = 0;
	while(num_tok < max_tok && *buf != '\n')
	{
		if(*buf == ' ') 
		{
			token[num_tok][off_tok] = 0;
			while(*buf == ' ') buf++;
			off_tok = 0;
			num_tok++;
		}
		else
		{
			token[num_tok][off_tok] = *buf;
			off_tok++;
			buf++;
		}
	}
	token[num_tok][off_tok] = 0;
	num_tok++;


	return num_tok;
}

int generic_read(int fd, void *buf, size_t len)
{
	struct ssufile *cursor;
	uint16_t *pos =  &(cur_process->file[fd]->pos);

	if( (cursor = cur_process->file[fd]) == NULL)
		return -1;

	if (~cursor->flags & O_RDONLY)
		return -1;
	
	if (*pos + len > cursor->inode->sn_size)
		len = cursor->inode->sn_size - *pos;

	file_read(cur_process->file[fd]->inode,*pos,buf,len);
	*pos += len;
	//printk("in generic read : %d \n", *pos);
	return len;
}

int generic_write(int fd, void *buf, size_t len)
{
	struct ssufile *cursor;
	uint16_t *pos =  &(cur_process->file[fd]->pos);

	if( (cursor = cur_process->file[fd]) == NULL)
		return -1;

	if (~cursor->flags & O_WRONLY)
		return -1;

	file_write(cur_process->file[fd]->inode,*pos,buf,len);
	*pos += len;
	//printk("in generic write : %d \n", *pos);
	return len;
}

int generic_lseek(int fd, int offset, int whence, char *opt)
{
	struct ssufile *cursor;
	uint16_t *pos = &(cur_process->file[fd]->pos);
	int location;
	int file_size;
	int flag;
	int diff;
	int i;
	char buf[BUFSIZ];

	//cursor가 가리키고 있는 ssufile 구조체가 없을 경우
	if( (cursor = cur_process->file[fd]) == NULL)
		return -1;

	//option에 따라 플래그 설정
	if(opt == NULL)
		flag = NO_OPT;
	else if(!strcmp(opt, "-e"))
		flag = E_OPT;
	else if(!strcmp(opt, "-a"))
		flag = A_OPT;
	else if(!strcmp(opt, "-re"))
		flag = RE_OPT;
	else if(!strcmp(opt, "-c"))
		flag = C_OPT;

	//cursor가 가리키는 파일의 크기
	file_size = cursor->inode->sn_size;

	switch(whence) {
		case SEEK_END:
			//파일 크기 = 파일 끝 위치
			location = file_size;
			break;
		case SEEK_SET:
			//파일 처음 위치
			location = 0;
			break;
		case SEEK_CUR:
			//현재 위치
			location = *pos;
			break;
	}

	if(flag == A_OPT) {
		bool is_negative = false;

		if(offset < 0) {
			is_negative = true;
			offset = -offset;
		}

		memset(buf, 0x00, sizeof(buf));
		*pos = 0;
		read(fd, buf, file_size);

		*pos = 0;
		for(i = 0; i < location; i++)
			write(fd, &buf[i], 1);

		for(i = 0; i < offset; i++)
			write(fd, "0", 1);

		for(i = location; i < file_size; i++)
			write(fd, &buf[i], 1);

		if(is_negative)
			offset = 0;
	}
	//whence에 해당하는 위치에 offset 적용
	location += offset;

	//시작 범위나 끝 범위를 벗어날 경우
	if(location < 0) {
		if(flag == RE_OPT) {
			*pos = 0;
			
			memset(buf, 0x00, BUFSIZ);
			read(fd, buf, file_size);

			*pos = 0;
			diff = -location;

			for(i = 0; i < diff; i++)
				write(fd, "0", 1);
			write(fd, buf, file_size);

			cursor->inode->sn_size += diff;
			file_size = cursor->inode->sn_size;
			*pos = 0;
		}
		else if(flag == C_OPT) {
			location += file_size;
			*pos = location;
		}
		else if(flag == NO_OPT || flag == E_OPT)
			return -1;
	}
	else if(location > file_size) {
		if(flag == E_OPT) {
			diff = location - file_size;
			*pos = file_size;
			for(i = 0; i < diff; i++)
				write(fd, "0", 1);
			cursor->inode->sn_size = location;
			file_size = cursor->inode->sn_size;
		}
		else if(flag == C_OPT) {
			*pos = location % file_size;
		}
		else if(flag == NO_OPT || flag == RE_OPT)
			return -1;
	}
	else
		*pos = location;

	return *pos;
}
