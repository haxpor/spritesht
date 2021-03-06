
#include <spritesht_lib.h>

#include <png.h>
#include <stdlib.h>
#include <string.h>

bool _sys_load_png(const char *name, whitgl_int *width, whitgl_int *height, unsigned char **data);
bool _sys_save_png(const char *name, whitgl_int width, whitgl_int height, unsigned char *data);

spritesht_spritesheet spritesht_create(whitgl_int max)
{
	spritesht_spritesheet sheet;
	sheet.max_sprites = max;
	sheet.num_sprites = 0;
	sheet.sprites = malloc(sizeof(spritesht_sprite) * max);
	whitgl_int i;
	for(i=0; i<sheet.max_sprites; i++)
		sheet.sprites[i].data = NULL;
	sheet.size = whitgl_ivec_zero;
	return sheet;
}
void spritesht_free(spritesht_spritesheet* sheet)
{
	whitgl_int i;
	for(i=0; i<sheet->num_sprites; i++)
		if(sheet->sprites[i].data)
			free(sheet->sprites[i].data);
	free(sheet->sprites);
}

bool _spritesht_is_clear(spritesht_sprite* sprite, whitgl_ivec pos)
{
	whitgl_int index = pos.x+pos.y*sprite->original_size.x;
	return *(sprite->data+index*4+3);
}

bool spritesht_add_sprite(spritesht_spritesheet* sheet, const char* file)
{
	if(sheet->num_sprites >= sheet->max_sprites)
		return false;
	spritesht_sprite* sprite = &sheet->sprites[sheet->num_sprites];
	if(!_sys_load_png(file, &sprite->size.x, &sprite->size.y, &sprite->data))
		return false;
	sprite->original_size = sprite->size;
	sprite->offset = whitgl_ivec_zero;
	strncpy(sprite->filename, file, 254);
	sprite->filename[255] = '\0';

	whitgl_ivec p;
	whitgl_int top = 0;
	whitgl_int right = sprite->original_size.x;
	whitgl_int bottom = sprite->original_size.y;
	whitgl_int left = 0;

	for(p.x=0; p.x<sprite->size.x; p.x++)
	{
		bool all_clear = true;
		for(p.y=0; p.y<sprite->size.y; p.y++)
			all_clear &= !_spritesht_is_clear(sprite, p);
		if(!all_clear)
			break;
		left++;
	}
	for(p.y=0; p.y<sprite->size.y; p.y++)
	{
		bool all_clear = true;
		for(p.x=0; p.x<sprite->size.x; p.x++)
			all_clear &= !_spritesht_is_clear(sprite, p);
		if(!all_clear)
			break;
		top++;
	}
	for(p.x=sprite->size.x-1; p.x>=left; p.x--)
	{
		bool all_clear = true;
		for(p.y=0; p.y<sprite->size.y; p.y++)
			all_clear &= !_spritesht_is_clear(sprite, p);
		if(!all_clear)
			break;
		right--;
	}
	for(p.y=sprite->size.y-1; p.y>=top; p.y--)
	{
		bool all_clear = true;
		for(p.x=0; p.x<sprite->size.x; p.x++)
			all_clear &= !_spritesht_is_clear(sprite, p);
		if(!all_clear)
			break;
		bottom--;
	}

	sprite->offset.x = left;
	sprite->offset.y = top;
	sprite->size.x = right-left;
	sprite->size.y = bottom-top;

	sheet->num_sprites++;
	return true;
}

typedef struct cell
{
	whitgl_ivec size;
	whitgl_ivec pos;
	struct cell* next;
} spritesht_cell;

bool spritesht_layout_to_size(spritesht_spritesheet* sheet, whitgl_ivec sheet_size, whitgl_int margin)
{
	whitgl_int i;
	spritesht_cell initial = {sheet_size, whitgl_ivec_zero, NULL};
	spritesht_cell* cells = malloc(sizeof(spritesht_cell));
	*cells = initial;
	for(i=0; i<sheet->num_sprites; i++)
	{
		whitgl_ivec size = {sheet->sprites[i].size.x+margin*2, sheet->sprites[i].size.y+margin*2};
		if(!cells)
			return false;
		spritesht_cell* iter = cells;
		spritesht_cell* c = NULL;
		while(iter)
		{
			bool ok = true;
			if(iter->size.x < size.x) ok = false;
			if(iter->size.y < size.y) ok = false;
			bool better = false;
			if(!c) better = true;
			if(c && iter->size.x < c->size.x) better = true;
			if(c && iter->size.y < c->size.y) better = true;
			if(ok && better)
				c = iter;
			iter = iter->next;
		}
		if(!c)
			return false;

		sheet->sprites[i].pos.x = c->pos.x+margin;
		sheet->sprites[i].pos.y = c->pos.y+margin;

		spritesht_cell old = *c;
		if(old.size.y > size.y)
		{
			c->size.y = size.y;
			spritesht_cell newcell = {{old.size.x, old.size.y-c->size.y}, {old.pos.x, old.pos.y+c->size.y}, cells};
			cells = malloc(sizeof(spritesht_cell));
			*cells = newcell;
		}
		old = *c;
		if(old.size.x > size.x)
		{
			c->size.x = size.x;
			spritesht_cell newcell = {{old.size.x-c->size.x, old.size.y}, {old.pos.x+c->size.x, old.pos.y}, cells};
			cells = malloc(sizeof(spritesht_cell));
			*cells = newcell;
		}
		if(cells == c)
		{
			cells = c->next;
			free(c);
		} else
		{
			iter = cells;
			while(iter->next != c)
				iter = iter->next;
			spritesht_cell* next = iter->next->next;
			free(iter->next);
			iter->next = next;
		}
	}
	while(cells)
	{
		spritesht_cell* next = cells->next;
		free(cells);
		cells = next;
	}
	return true;
}

int _spritesht_cmpfunc (const void * va, const void * vb)
{
	spritesht_sprite* a = (spritesht_sprite*)va;
	spritesht_sprite* b = (spritesht_sprite*)vb;
	return (b->size.y-a->size.y)*1000 + (b->size.x-a->size.x);
}

bool spritesht_layout(spritesht_spritesheet* sheet, whitgl_int max_image_dimension, whitgl_int margin)
{
	whitgl_int i;
	whitgl_ivec size = {1,1};

	qsort(sheet->sprites, sheet->num_sprites, sizeof(spritesht_sprite), _spritesht_cmpfunc);

	bool width_next = true;
	while(true)
	{
		if(spritesht_layout_to_size(sheet, size, margin))
			break;
		if(width_next) size.x <<= 1;
		else size.y <<= 1;
		width_next = !width_next;
		if(size.x > max_image_dimension)
			return false;
	}
	sheet->size = size;
	return true;
}

bool spritesht_save_image(spritesht_spritesheet* sheet, const char* file)
{
	whitgl_ivec size = sheet->size;
	unsigned char* out_data = malloc(size.x*size.y*4);
	memset(out_data, '\0', size.x*size.y*4);

	whitgl_int i;
	for(i=0; i<sheet->num_sprites; i++)
	{
		spritesht_sprite s = sheet->sprites[i];
		whitgl_ivec p = s.pos;
		whitgl_int row;
		for(row=0; row<s.size.y; row++)
		{
			whitgl_int index = (row+s.offset.y)*s.original_size.x+s.offset.x;
			unsigned char* src_start = &s.data[index*4];
			unsigned char* dst_start = &out_data[(p.x+(p.y+row)*size.x)*4];
			memcpy(dst_start, src_start, 4*s.size.x);
		}
	}

	bool success = _sys_save_png(file, size.x, size.y, out_data);
	free(out_data);
	return success;
}

bool spritesht_save_meta(spritesht_spritesheet* sheet, const char* file)
{
	spritesht_spritesheet save = spritesht_create(sheet->num_sprites);
	whitgl_int i;
	save.num_sprites = sheet->num_sprites;
	for(i=0; i<sheet->num_sprites; i++)
	{
		save.sprites[i] = sheet->sprites[i];
		save.sprites[i].data = NULL;
	}
	FILE *fp = fopen(file, "wb");
	if(!fp)
		return false;
	fwrite(&spritesht_magic_value, sizeof(whitgl_int), 1, fp);
	fwrite(&sheet->num_sprites, sizeof(whitgl_int), 1, fp);
	fwrite(save.sprites, sizeof(spritesht_sprite), sheet->num_sprites, fp);
	fclose(fp);
	return true;
}
bool spritesht_load_meta(spritesht_spritesheet* sheet, const char* file)
{
	FILE *fp = fopen(file, "rb");
	if(!fp)
		return false;
	whitgl_int magic;
	fread(&magic, sizeof(whitgl_int), 1, fp);
	if(magic != spritesht_magic_value)
		return false;
	whitgl_int size;
	fread(&size, sizeof(whitgl_int), 1, fp);
	*sheet = spritesht_create(size);
	sheet->num_sprites = size;
	fread(sheet->sprites, sizeof(spritesht_sprite), size, fp);
	fclose(fp);
	return true;
}
bool spritesht_save_meta_as_csv(spritesht_spritesheet* sheet, const char* file)
{
	FILE *fp = fopen(file, "w");
	if(!fp)
		return false;
	fprintf(fp, "# Size\n");
	fprintf(fp, "%d\n", (int)sheet->num_sprites);
	whitgl_int i;
	fprintf(fp, "# Filename, X Size, Y Size, X Pos, Y Pos, X Off, Y Off, X Orig, Y Orig\n");
	for(i=0; i<sheet->num_sprites; i++)
	{
		spritesht_sprite s = sheet->sprites[i];
		fprintf(fp, "%s,%d,%d,%d,%d,%d,%d,%d,%d\n", s.filename, (int)s.size.x, (int)s.size.y,
			                                        (int)s.pos.x, (int)s.pos.y,
			                                        (int)s.offset.x, (int)s.offset.y,
			                                        (int)s.original_size.x, (int)s.original_size.y);
	}
	fclose(fp);
	return true;
}

bool _sys_load_png(const char *name, whitgl_int *width, whitgl_int *height, unsigned char **data)
{
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned int sig_read = 0;
	int color_type, interlace_type;
	FILE *fp;
	if ((fp = fopen(name, "rb")) == NULL)
		return false;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL)
	{
		fclose(fp);
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		fclose(fp);
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		// setjmp triggered while processing png, so we had a problem reading
		// the file, clean up and give up
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		return false;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, sig_read);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);

	png_uint_32 read_width, read_height;
	int bit_depth;
	png_get_IHDR(png_ptr, info_ptr, &read_width, &read_height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
	if(width)
		*width = read_width;
	if(height)
		*height = read_height;
	bool has_alpha = color_type & PNG_COLOR_MASK_ALPHA;
	unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	if(data)
	{
		unsigned char* load = (unsigned char*) malloc(row_bytes * read_height);
		png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);

		png_uint_32 i;
		for (i = 0; i < read_height; i++)
		{
			memcpy(load+(row_bytes * i), row_pointers[i], row_bytes);
		}

		*data = (unsigned char*)malloc(read_width*read_height*4);
		for(i=0; i<read_width*read_height; i++)
		{
			whitgl_int dst_index = i*4;
			whitgl_int src_index = has_alpha ? i*4 : i*3;
			*(*data+dst_index) = load[src_index+0];
			*(*data+dst_index+1) = load[src_index+1];
			*(*data+dst_index+2) = load[src_index+2];
			if(has_alpha)
				*(*data+dst_index+3) = load[src_index+3];
			else
				*(*data+dst_index+3) = 0xff;
		}
		free(load);
	}
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(fp);

	return true;
}
bool _sys_save_png(const char *name, whitgl_int width, whitgl_int height, unsigned char *data)
{
	FILE *fp = fopen(name, "wb");
	if(!fp) return false;

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) return false;

	png_infop info = png_create_info_struct(png);
	if (!info) return false;

	if (setjmp(png_jmpbuf(png))) return false;

	png_init_io(png, fp);

	png_uint_32 png_width = width;
	png_uint_32 png_height = height;

	png_set_IHDR(png, info, png_width, png_height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png, info);

	png_byte** row_pointers = png_malloc (png, png_height*sizeof(png_byte*));
	whitgl_int y;
	for (y = 0; y < height; ++y)
	{
		whitgl_int size = sizeof(uint8_t)*png_width*4;
		row_pointers[y] = &data[size*y];
	}

	png_write_image(png, row_pointers);
	png_write_end(png, NULL);

	fclose(fp);

	return true;
}
