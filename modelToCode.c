/*
**根据输入的数据模型文件生成节点映射文件及回调函数文件
**如输入数据模型文件为ethernet.txt，输出文件为:
**ethernet_table.c
**ethernet_func.c
**
**数据模型文件ethernet.txt内容如下：
**Device.Ethernet.	object	R
**	InterfaceNumberOfEntries	unsignedInt	R
**	LinkNumberOfEntries	unsignedInt	R
**	VLANTerminationNumberOfEntries	unsignedInt	R
**	RMONStatsNumberOfEntries	unsignedInt	R
**Device.Ethernet.Interface.{i}.	object(0:)	R
**	Enable	boolean	W
**	Status	string	R
**	Alias	string(:64)	W
**	Name	string(:64)	R
**	LastChange	unsignedInt	R
**	LowerLayers	string[](:1024)	W
**	Upstream	boolean	R
**	MACAddress	string(:17)	R
**	MaxBitRate	int(-1:)	W
**	CurrentBitRate	unsignedInt	R
**	DuplexMode	string	W
**	EEECapability	boolean	R
**	EEEEnable	boolean	W
**
**
*/

#include<stdio.h>
#include<string.h>


enum{
	M_PATH,		/*path*/
	M_DTYPE,	/*data type*/
	M_WRATTR,	/*read write attribute*/
	M_MAX,
};

/*
检查节点路径是否是以“{i}.”结尾，以判断该节点是否是对象节点——多实例节点。
*/
int multi_instance_node(const char *nodepath){
	int path_len = 0;
	int suffix_len = 0;
	char *suffix = "{i}.";

	if(nodepath == NULL){
		printf("[%s][%d]parameter error!\n",__FUNCTION__,__LINE__);
		return 0;
	}
	path_len = strlen(nodepath);
	suffix_len = strlen(suffix);

	printf("path = [%s]\n", nodepath);
	if(path_len < suffix_len){
		printf("[%s][%d]parameter error!\n",__FUNCTION__,__LINE__);
		return 0;
	}
	/*compare the last part.*/
	if(strcmp(nodepath + path_len - suffix_len, suffix) == 0){
		printf("[%s][%d]Multi Instance Node!\n",__FUNCTION__,__LINE__);
		return 1;
	}
	printf("[%s][%d]Not Multi Instance Node!\n",__FUNCTION__,__LINE__);
	return 0;
}

int create_func_name(const char *objpath, const char *pampath, const char *wrattr, char *func_get,char *func_set){
	char path_segment[8][128] = {{0}};
	char *token = NULL;
	int i = 0;
	int j = 0;
	char func_base[128] = {0};
	char object_path[128] = {0};

	strcpy(object_path,objpath);

	token = strtok(object_path,".");
	while((token != NULL) && (i < 8)){
		strcpy(path_segment[i],token);
		i++;
		token = strtok(NULL, ".");
	}

	for(j=0; j<i;j++){
		if(strcmp(path_segment[j],"Device") && strcmp(path_segment[j], "{i}")){
			strcat(func_base,path_segment[j]);
		}
	}
	if(strncmp(wrattr,"W", 1) == 0){/*READ_WRITE*/
		sprintf(func_get,"Get_%s%s", func_base, pampath);
		sprintf(func_set,"Set_%s%s", func_base, pampath);
	}else{/*READ_ONLY*/
		sprintf(func_get,"Get_%s%s", func_base, pampath);
		sprintf(func_set,"NULL" );
	}

	return 0;
}
int replace_i_to_symbol(char *str){
	char buffer[100];
	char *pos = strstr(str, "{i}");
	if (pos) {
		strncpy(buffer, str, pos - str);
		buffer[pos - str] = '\0';
		strcat(buffer, "%d");
		strcat(buffer, pos + 3);
		strcpy(str, buffer);
	}
}
/*
**create_calbacks
**根据节点路径，节点参数名，GET,SET函数名，填充GET,SET回调函数体。
**结果写入fp指向的文件。
*/
int create_calbacks(const char *path, const char *param, const char *funcget,const char *funcset,FILE *fp, FILE *fphd){

	char real_path[128] = {0};
	int multi_instance = 0;

	sprintf(real_path,"%s%s",path,param);/*result:Device.Ethernet.Interface.{i}.Enable*/

	if(strstr(path, "{i}") != NULL){
		multi_instance = 1;
		replace_i_to_symbol(real_path);/*result:Device.Ethernet.Interface.%d.Enable*/
	}

	if(strcmp(funcget, "NULL")){/*Generate the Get_XXX*/
		fprintf(fp, "int %s(dm_req_t *req, char *buf, int len){\n", funcget);
		fprintf(fp, "    int err = 0;\n");
		fprintf(fp, "    char path[MAX_DM_PATH] = {0};\n");
		fprintf(fp, "    char result[32] = {0};\n");
		if(multi_instance)
			fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1);\n",real_path);
		else
			fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\");\n",real_path);
		fprintf(fp, "    err = ty_mdm_get(path, result);\n");
		fprintf(fp, "    if(!err){\n");
		fprintf(fp, "        USP_STRNCPY(buf,result,len);\n");
		fprintf(fp, "    }\n");
		fprintf(fp, "    return err;\n");
		fprintf(fp, "}\n");
		//for header.
		fprintf(fphd,"int %s(dm_req_t *req, char *buf, int len);\n", funcget);
	}
	if(strcmp(funcset, "NULL")){/*Generate the Set_XXX*/
		fprintf(fp, "int %s(dm_req_t *req, char *buf){\n", funcset);
		fprintf(fp, "    int err = 0;\n");
		fprintf(fp, "    char path[MAX_DM_PATH] = {0};\n");
		if(multi_instance)
			fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1);\n",real_path);
		else
			fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\");\n",real_path);
		fprintf(fp, "    err = ty_mdm_set(path, buf);\n");
		fprintf(fp, "    if(err){\n");
		fprintf(fp, "        printf(\"Set %s error.\\n\");\n",param);
		fprintf(fp, "    }\n");
		fprintf(fp, "    return err;\n");
		fprintf(fp, "}\n");
		//Write to header file.
		fprintf(fphd,"int %s(dm_req_t *req, char *buf);\n", funcset);
	}
	return 0;
}
/*
**根据数据模型文件中的数据类型，找出obuspa中对应的类型字符串。
*/
void data_type_translate(const char* datatype,char *type){
	
	if(strncmp(datatype,"unsignedInt",11)==0){
		strcpy(type,"DM_UINT"); 
	}else if(!strncmp(datatype,"unsignedLong",12)){
		strcpy(type,"DM_ULONG"); 
	}else if(!strncmp(datatype,"boolean",7)){
		strcpy(type,"DM_BOOL"); 
	}else if(!strncmp(datatype,"string",6)){
		strcpy(type,"DM_STRING"); 
	}else if(!strncmp(datatype,"int",3)){
		strcpy(type,"DM_INT"); 
	}else if(!strncmp(datatype,"base64",6)){
		strcpy(type,"DM_BASE64"); 
	}else if(!strncmp(datatype,"hexBinary",9)){
		strcpy(type,"DM_HEXBIN"); 
	}else if(!strncmp(datatype,"decimal",7)){
		strcpy(type,"DM_DECIMAL"); 
	}else if(!strncmp(datatype,"long",4)){
		strcpy(type,"DM_LONG"); 
	}

}
/*
**将数据模型文件翻译成C代码：
**1.节点回调函数映射表
**2.GET,SET函数实现
*/
int translate(const char *fileinput,const char *filetable,const char *filefunc,const char *fileheader){
	FILE *fp_in = NULL;
	FILE *fp_table = NULL;
	FILE *fp_func = NULL;
	FILE *fp_header = NULL;
	char line[1024] = {0};
	char parameter[M_MAX][256] = {{0}};
	char object[M_MAX][256] = {{0}};
	int line_count = 0;
	char line_new[1024] = {0};

	int new_content = 0;

	char read_write[16] = {0};
	char func_name_get[128] = {0};
	char func_name_set[128] = {0};
	char data_type[128] = {0};

	fp_in = fopen(fileinput,"r");
	if(fp_in == NULL){
		return -1;
	}

	fp_table = fopen(filetable, "w");
	if(fp_table == NULL){
		return -1;
	}

	fp_func = fopen(filefunc, "w");
	if(fp_func == NULL){
		return -1;
	}

	fp_header = fopen(fileheader, "w");
	if(fp_header == NULL){
		return -1;
	}
	

	while(fgets(line,sizeof(line),fp_in)){
		new_content = 0;
		//printf("line get[%s]\n",line);
		if(strstr(line,"Device") && strstr(line,"object")){/*对象节点*/
			printf("Object get:[%s]\n",line);
			sscanf(line,"%s %s %s",object[M_PATH],object[M_DTYPE],object[M_WRATTR]);

			printf("object_path[%s],datatype[%s],attribute[%s]\n",object[M_PATH],object[M_DTYPE],object[M_WRATTR]);
			
			if(multi_instance_node(object[M_PATH])){/*该节点是以“{i}.”结尾，那么需要加到注册表中以对该对象注册*/
				/*need to generate the register code for the object.*/
				snprintf(line_new,sizeof(line_new),"{\"%s\", NULL, NULL, NULL, NULL, NULL, NULL, 0, MULTI_INSTANCE},",object[M_PATH]);
				new_content = 1;
			}

		}else{/*普通参数节点.*/
			sscanf(line,"%s %s %s",parameter[M_PATH],parameter[M_DTYPE],parameter[M_WRATTR]);

			/*Generate the call-back functions*/
			
			//create the function name.
			create_func_name(object[M_PATH],parameter[M_PATH],parameter[M_WRATTR],func_name_get,func_name_set);
			//printf("GetFuncName[%s],SetFuncName[%s]\n", func_name_get, func_name_set);
			//create the function.
			create_calbacks(object[M_PATH], parameter[M_PATH],func_name_get,func_name_set,fp_func,fp_header);


			/*Generate the TY_USP_FUNC_TABLE*/
			if(strncmp(parameter[M_WRATTR],"R",1)==0){
				strcpy(read_write,"READ_ONLY");
			}else if(strncmp(parameter[M_WRATTR],"W",1)==0){
				strcpy(read_write,"READ_WRITE");
			}
			data_type_translate(parameter[M_DTYPE],data_type);
			
			snprintf(line_new,sizeof(line_new),"{\"%s%s\", NULL, %s, %s, NULL, NULL, NULL, %s, %s},", object[M_PATH], parameter[M_PATH], func_name_get, func_name_set, data_type, read_write);
			new_content = 1;
		}

		line_count ++;


		if(new_content){
			fprintf(fp_table, "%s\n", line_new);
		}

	}
	fprintf(fp_table, "{NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0},\n");
	printf("line number:%d\n",line_count);


	fclose(fp_in);
	fclose(fp_table);
	fclose(fp_func);
	fclose(fp_header);
	return 0;
}

int main(int argc ,char ** argv){

	char *dot = NULL;
	
	char file_input[128] = {0};
	char file_table[128] = {0};
	char file_func[128] = {0};
	char file_header[128] = {0};

	if(argc<2){
		printf("please enter a filename to be translated.\n");
		return -1;
	}
	/*get the filename which is going to be translated.*/
	strcpy(file_input,argv[1]);

	/*根据输入文件名，制作输出文件名*/
	strcpy(file_table,file_input);
	strcpy(file_func,file_input);
	strcpy(file_header,file_input);

	dot = strrchr(file_table,'.');
	if (dot != NULL) {
        strcpy(dot, "_table.c");  // 替换为新扩展名
    } else {
        strcat(file_table, "_table.c");  // 如果没有扩展名，则添加
    }

	dot = strrchr(file_func,'.');
	if (dot != NULL) {
        strcpy(dot, "_func.c");  // 替换为新扩展名
    } else {
        strcat(file_func, "_func.c");  // 如果没有扩展名，则添加
    }
	
	dot = strrchr(file_header,'.');
	if (dot != NULL) {
        strcpy(dot, "_header.h");  // 替换为新扩展名
    } else {
        strcat(file_header, "_header.c");  // 如果没有扩展名，则添加
    }
	printf("input[%s]\ntableFile[%s]\nfuncFile[%s]", file_input, file_table,file_func,file_header);
	
	/*将数据模型文件翻译成C代码*/
	translate(file_input,file_table,file_func,file_header);

	return 0;

}

