/*
**根据输入的数据模型文件生成节点映射文件及回调函数文件
**如输入数据模型文件为ethernet.txt，输出文件为:
**ethernet_table.c
**ethernet_func.c
**
**数据模型文件ethernet.txt内容如下：
**Device.Ethernet.  object  R
**  InterfaceNumberOfEntries    unsignedInt R
**  LinkNumberOfEntries unsignedInt R
**  VLANTerminationNumberOfEntries  unsignedInt R
**  RMONStatsNumberOfEntries    unsignedInt R
**Device.Ethernet.Interface.{i}.    object(0:)  R
**  Enable  boolean W
**  Status  string  R
**  Alias   string(:64) W
**  Name    string(:64) R
**  LastChange  unsignedInt R
**  LowerLayers string[](:1024) W
**  Upstream    boolean R
**  MACAddress  string(:17) R
**  MaxBitRate  int(-1:)    W
**  CurrentBitRate  unsignedInt R
**  DuplexMode  string  W
**  EEECapability   boolean R
**  EEEEnable   boolean W
**
**
*/

#include<stdio.h>
#include<string.h>
#include<unistd.h> /*for sleep()*/
#include <ctype.h> /*for islower() toupper()*/

enum{
    M_PATH,     /*path*/
    M_DTYPE,    /*data type*/
    M_WRATTR,   /*read write attribute*/
    M_MAX,
};

/*Code file's content types definition*/
typedef enum{
    C_HEAD,                                          //file head
    C_CALLBACK_INST,                        //call back function implementation.
    C_CALLBACK_DECL,                        //call back funcionts declaration.
    C_CALLBACK_MAP_TABLE,            //TY_USP_FUNC_TABLE
    C_DYNAMIC_OBJ_MAP_TABLE,     //TY_DYNAMIC_OBJ_TABLE
    C_MAX,
}Content_Type;

/*
**Multiinstance series.
*/
enum{
    MULTY_INST_1 = 1,
    MULTY_INST_2,
    MULTY_INST_3,
    MULTY_INST_4,
    MULTY_INST_5,
    MULTY_INST_6,
    MULTY_INST_MAX,
};

int countOccurrences(const char *str, const char *sub) {
    int count = 0;
    const char *temp = str;
    while ((temp = strstr(temp, sub)) != NULL) {
        count++;
        temp++;  // Move past the current occurrence
    }
    return count;
}

/*
**将字符串中小写字母转化成大写字母。
**
*/
void toUpperCase(char *str) {
    while (*str) {
        if (islower(*str)) {
            *str = toupper(*str);
        }
        str++;
    }
}

/*
**将字符串中大写字母转化成小写字母。
**
*/
void toLowerCase(char *str) {
    while (*str) {
        if (isupper(*str)) {
            *str = tolower(*str);
        }
        str++;
    }
}

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

/*
**Replace "{i}" in the string with "%d".
*/
int replace_i_to_symbol(char *str){
    char buffer[256] = {0};;
    char *ptmp = str;
    char *pbuf = buffer;
    char *pos =NULL;

    while(1){
        char *pos = strstr(ptmp,"{i}");
        if(pos == NULL){
            strcpy(pbuf, ptmp);
            break;
        }
        memcpy(pbuf, ptmp, pos - ptmp);
        pbuf += pos - ptmp;

        memcpy(pbuf,"%d",2);
        pbuf += 2;

        ptmp = pos +3;

    }

    strcpy(str,buffer);

}
/*
**create_calbacks
**根据节点路径，节点参数名，GET,SET函数名，填充GET,SET回调函数体。
**结果写入fp指向的文件。
*/
int create_calbacks(const char *path, const char *param, const char *funcget, const char *funcset, FILE *fp, Content_Type ctype){

    char real_path[128] = {0};
    int multi_instance = 0;

    int instance_layers_count = 0;
    
    sprintf(real_path,"%s%s", path, param);/*result:Device.Ethernet.Interface.{i}.Enable*/

    instance_layers_count = countOccurrences(real_path,"{i}");

    if(strstr(path, "{i}") != NULL){
        replace_i_to_symbol(real_path);/*result:Device.Ethernet.Interface.%d.Enable*/
        multi_instance = 1;
    }

    printf("[%s][%d]real_path replaced[%s]\n", __FUNCTION__, __LINE__,real_path);
    if(ctype == C_CALLBACK_INST){
        if(strcmp(funcget, "NULL")){/*Generate the Get_XXX*/
            fprintf(fp, "int %s(dm_req_t *req, char *buf, int len){\n", funcget);
            fprintf(fp, "    int err = 0;\n");
            fprintf(fp, "    char path[MAX_DM_PATH] = {0};\n");
            fprintf(fp, "    char result[32] = {0};\n");

            if(multi_instance){
                switch(instance_layers_count){
                    case MULTY_INST_1:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1);\n",real_path);
                        break;
                    case MULTY_INST_2:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1, inst2);\n",real_path);
                        break;
                    case MULTY_INST_3:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1, inst2, inst3);\n",real_path);
                        break;
                    case MULTY_INST_4:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1, inst2, inst3, inst4);\n",real_path);
                        break;
                    case MULTY_INST_5:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1, inst2, inst3, inst4, inst5);\n",real_path);
                        break;
                    case MULTY_INST_6:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1, inst2, inst3, inst4, inst6);\n",real_path);
                        break;

                    default:
                        printf("[%s][%d]ERROR:Please check the Data-Model file!\n", __FUNCTION__, __LINE__);
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1);\n",real_path);
                        break;

                }
            }else
            fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\");\n",real_path);
            fprintf(fp, "    err = ty_mdm_get(path, result);\n");
            fprintf(fp, "    if(!err){\n");
            fprintf(fp, "        USP_STRNCPY(buf,result,len);\n");
            fprintf(fp, "    }\n");
            fprintf(fp, "    return err;\n");
            fprintf(fp, "}\n");
        }
        if(strcmp(funcset, "NULL")){/*Generate the Set_XXX*/
            fprintf(fp, "int %s(dm_req_t *req, char *buf){\n", funcset);
            fprintf(fp, "    int err = 0;\n");
            fprintf(fp, "    char path[MAX_DM_PATH] = {0};\n");

            if(multi_instance){
                switch(instance_layers_count){
                    case MULTY_INST_1:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1);\n",real_path);
                        break;
                    case MULTY_INST_2:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1, inst2);\n",real_path);
                        break;
                    case MULTY_INST_3:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1, inst2, inst3);\n",real_path);
                        break;
                    case MULTY_INST_4:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1, inst2, inst3, inst4);\n",real_path);
                        break;
                    case MULTY_INST_5:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1, inst2, inst3, inst4, inst5);\n",real_path);
                        break;
                    case MULTY_INST_6:
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1, inst2, inst3, inst4, inst6);\n",real_path);
                        break;

                    default:
                        printf("[%s][%d]ERROR:Please check the Data-Model file!\n", __FUNCTION__, __LINE__);
                        fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\", inst1);\n",real_path);
                        break;
                }
            }else
            fprintf(fp, "    USP_SNPRINTF(path, sizeof(path), \"%s\");\n",real_path);

            fprintf(fp, "    err = ty_mdm_set(path, buf);\n");
            fprintf(fp, "    if(err){\n");
            fprintf(fp, "        printf(\"Set %s error.\\n\");\n",param);
            fprintf(fp, "    }\n");
            fprintf(fp, "    return err;\n");
            fprintf(fp, "}\n");
        }
    }
    else if(ctype == C_CALLBACK_DECL){
        if(strcmp(funcget, "NULL")){/*Generate the Get_XXX*/
            fprintf(fp,"int %s(dm_req_t *req, char *buf, int len);\n", funcget);
        }
        if(strcmp(funcset, "NULL")){/*Generate the Set_XXX*/
            //Write to header file.
            fprintf(fp,"int %s(dm_req_t *req, char *buf);\n", funcset);
        }
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
    }else if(!strncmp(datatype,"dateTime",8)){
        strcpy(type, "DM_DATETIME");
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

int c_head_prepare(FILE *fp, char *module_name ){
    char line[128] = {0};
    fprintf(fp,"/*Header segment*/\n");
    fprintf(fp,"#include \"usp_api.h\"\n");
    fprintf(fp,"#include \"usp_err_codes.h\"\n");
    fprintf(fp,"#include \"common_defs.h\"\n");
    fprintf(fp,"#include \"data_model.h\"\n");
    fprintf(fp,"#include \"ty_usp.h\"\n");
    fprintf(fp,"#include \"ty_%s.h\"\n", module_name);
    fprintf(fp,"\n");
    fprintf(fp,"/*Function segment*/\n");
    return 0;

}
int c_tail_prepare(FILE *fp, char *module_name){
    char line[128] = {0};

    //fseek(fp, 0, SEEK_END);
    printf("[%s][%d]Trace line\n", __FUNCTION__, __LINE__);

    fprintf(fp,"/*Regester segment*/\n");
    fprintf(fp,"int register_%s_datamodel(void){\n", module_name);
    fprintf(fp,"    int err = USP_ERR_OK;\n");
    fprintf(fp,"    int i = 0;\n");
    fprintf(fp,"    TY_USP_FUNC_TABLE %s_parm;\n", module_name);
    fprintf(fp,"    for(i = 0; %s_node_table[i].path != NULL; i++)\n", module_name);
    fprintf(fp,"    {\n");
    fprintf(fp,"        %s_parm = %s_node_table[i];\n", module_name, module_name);
    fprintf(fp,"        err |=Ty_Register_DataModel(%s_parm);\n", module_name);
    fprintf(fp,"    }\n");
    fprintf(fp,"    return err;\n");
    fprintf(fp,"}\n");

//write int register_%s_object(void)

    printf("[%s][%d]Trace line\n", __FUNCTION__, __LINE__);
    fprintf(fp,"int register_%s_object(void){\n", module_name);
    fprintf(fp,"    int err = USP_ERR_OK;\n");
    fprintf(fp,"    int i = 0;\n");
    fprintf(fp,"    TY_DYNAMIC_OBJ_TABLE %s_table = {0};\n", module_name);
    fprintf(fp,"    for(i = 0; %s_dynamic_object_map_table[i].base_path != NULL; i++)\n", module_name);
    fprintf(fp,"    {\n");
    fprintf(fp,"        %s_table = %s_dynamic_object_map_table[i];\n", module_name, module_name);
    fprintf(fp,"        err |=Ty_Register_ObjDataModel(%s_table);\n", module_name);
    fprintf(fp,"    }\n");
    fprintf(fp,"    if(err != USP_ERR_OK){\n");
    fprintf(fp,"        err = USP_ERR_DEREGISTER_FAILURE;\n");
    fprintf(fp,"    }\n");
    fprintf(fp,"    return err;\n");
    fprintf(fp,"}\n");
    printf("[%s][%d]Trace line\n", __FUNCTION__, __LINE__);
    return 0;
}

int h_head_prepare(FILE *fp, char *module_name){
   
    //printf("[%s][%d]Trace line\n", __FUNCTION__, __LINE__);
    
    /*Here should change the module_name to upper.*/
    toUpperCase(module_name);

    fprintf(fp,"#ifndef __TY_%s_H__\n",module_name);
    fprintf(fp,"#define __TY_%s_H__\n",module_name);

    /*for the function name we use the lowercase letters */
    toLowerCase(module_name);

    fprintf(fp,"\n");
    fprintf(fp,"extern TY_USP_FUNC_TABLE %s_node_table[];\n",module_name);
    fprintf(fp,"extern TY_DYNAMIC_OBJ_TABLE %s_dynamic_object_map_table[];\n",module_name);
    fprintf(fp,"int register_%s_datamodel(void);\n",module_name);
    fprintf(fp,"int register_%s_object(void);\n",module_name);
    fprintf(fp,"\n");


    return 0;
}
int h_tail_prepare(FILE *fp, char *module_name){
    fprintf(fp, "\n#endif");
    //printf("[%s][%d]Trace line\n", __FUNCTION__, __LINE__);
    return 0;
}

/*
**Scan the data-model file to create the C code to ty_xxx.c and ty_xxx.h.
**
*/
int model_content_translate( FILE *fp_in, FILE *fp_out, Content_Type ctype, char *module_name){

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

    printf("\n\n[%s][%d]ctype=%d\n",__FUNCTION__, __LINE__, ctype);

    /*make sure that the data module be read from the head.*/
    fseek(fp_in, 0, SEEK_SET);/*多次打开数据模型文件，必须每次从头开始读取，否则除第一次操作外其他操作无法进行*/
    fseek(fp_out, 0, SEEK_END);/*多次写入文件，必须保证每次写前都从文件末尾开始写，否则会出现后续内容无法写入的问题*/

    /*if content typ is aobout the TABLE,write the table array head*/
    switch(ctype){
        case C_CALLBACK_MAP_TABLE:
            printf("\n\n[%s][%d]write the C_CALLBACK_MAP_TABLE\n",__FUNCTION__, __LINE__);
            fprintf(fp_out, "TY_USP_FUNC_TABLE %s_node_table[] ={\n", module_name);
            break;
        case C_DYNAMIC_OBJ_MAP_TABLE:
            printf("\n\n[%s][%d]write the C_DYNAMIC_OBJ_MAP_TABLE\n",__FUNCTION__, __LINE__);
            fprintf(fp_out, "TY_DYNAMIC_OBJ_TABLE %s_dynamic_object_map_table[]={\n", module_name);
            break;
        default:
            printf("[%s][%d]Trace line\n", __FUNCTION__, __LINE__);
            break;
    }

    while(fgets(line, sizeof(line), fp_in)){
        new_content = 0;
        if(strstr(line,"Device") && strstr(line,"object")){/*对象节点*/
            //printf("[%s][%d]Object get:[%s]\n",__FUNCTION__, __LINE__,line);
            sscanf(line,"%s %s %s",object[M_PATH],object[M_DTYPE],object[M_WRATTR]);
            printf("[%s][%d]object_path[%s],datatype[%s],attribute[%s]\n",__FUNCTION__, __LINE__, object[M_PATH],object[M_DTYPE],object[M_WRATTR]);
            if(ctype == C_CALLBACK_MAP_TABLE){
                if(multi_instance_node(object[M_PATH])){/*该节点是以“{i}.”结尾，那么需要加到注册表中以对该对象注册*/

                    /*Need to generate the register code for the object.*/
                    /*Maybe here should delete the last dot of the object[M_PATH]*/
                    fprintf(fp_out, "{\"%s\", NULL, NULL, NULL, NULL, NULL, NULL, 0, MULTI_INSTANCE},\n", object[M_PATH]);
                }
            }
        }else{/*普通参数节点.*/
            sscanf(line, "%s %s %s", parameter[M_PATH], parameter[M_DTYPE], parameter[M_WRATTR]);

            /*Generate the call-back functions*/
            if(ctype == C_CALLBACK_MAP_TABLE ||ctype == C_CALLBACK_INST || ctype == C_CALLBACK_DECL){
                //create the function name.
                create_func_name(object[M_PATH], parameter[M_PATH], parameter[M_WRATTR], func_name_get, func_name_set);
                printf("[%s][%d]GetFuncName[%s],SetFuncName[%s]\n", __FUNCTION__, __LINE__, func_name_get, func_name_set);
                if(ctype == C_CALLBACK_INST || ctype == C_CALLBACK_DECL){
                    //create the function or header file.
                    printf("[%s][%d]    object[%s]  parm[%s]\n", __FUNCTION__, __LINE__, object[M_PATH], parameter[M_PATH]);
                    create_calbacks(object[M_PATH], parameter[M_PATH], func_name_get, func_name_set, fp_out, ctype);
                }else{
                    /*Generate the TY_USP_FUNC_TABLE*/
                    if(strncmp(parameter[M_WRATTR], "R", 1)==0){
                        strcpy(read_write, "READ_ONLY");
                    }else if(strncmp(parameter[M_WRATTR], "W", 1)==0){
                        strcpy(read_write, "READ_WRITE");
                    }
                    data_type_translate(parameter[M_DTYPE], data_type);

                    //snprintf(line_new,sizeof(line_new),"{\"%s%s\", NULL, %s, %s, NULL, NULL, NULL, %s, %s},", object[M_PATH], parameter[M_PATH], func_name_get, func_name_set, data_type, read_write);
                    fprintf(fp_out,"{\"%s%s\", NULL, %s, %s, NULL, NULL, NULL, %s, %s},\n", object[M_PATH], parameter[M_PATH],
                                     func_name_get, func_name_set, data_type, read_write);
                }
            }

            else if(ctype == C_DYNAMIC_OBJ_MAP_TABLE){/*多实例数量参数获取，以制作动态节点映射表TY_DYNAMIC_OBJ_TABLE*/
                if(strstr(parameter[M_PATH], "NumberOfEntries")){
                    //printf("[%s][%d]Parameter=[%s]\n", __FUNCTION__, __LINE__,parameter[M_PATH]);
                    /*Get the node name frme the parameter name.*/

                    char dest_node[64] = {0};
                    char *p = NULL;
                    p = strstr(parameter[M_PATH], "NumberOfEntries");
                    strncpy(dest_node, parameter[M_PATH], p -parameter[M_PATH]);

                    /*Delete the last dot of the object[M_PATH].*/
                    p = strrchr(object[M_PATH], '.');
                    if(p != NULL){
                        *p= '\0';
                    }

                    //sscanf(parameter[M_PATH], "%sNumberOfEntries", dest_node);
                    printf("[%s][%d]Dest Node=[%s],Object_path[%s]\n", __FUNCTION__, __LINE__,dest_node, object[M_PATH]);
                    //snprintf(line_new,sizeof(line_new),"{\"%\", \"%s\",\"%s\"},",object[M_PATH],dest_node,parameter[M_PATH]);
                    fprintf(fp_out, "{\"%s\", \"%s\", \"%s\"},\n", object[M_PATH], dest_node, parameter[M_PATH]);

                }
            }
        }
        line_count ++;

    }

    printf("[%s][%d] line number:%d\n",__FUNCTION__, __LINE__, line_count);

    /*write the table array tail*/
    switch(ctype){
        case C_CALLBACK_MAP_TABLE:
            printf("\n\n[%s][%d]write the C_CALLBACK_MAP_TABLE\n",__FUNCTION__, __LINE__);
            fprintf(fp_out, "{NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0},\n");
            fprintf(fp_out, "};\n");
            break;
        case C_DYNAMIC_OBJ_MAP_TABLE:
            printf("\n\n[%s][%d]write the C_DYNAMIC_OBJ_MAP_TABLE\n",__FUNCTION__, __LINE__);
            fprintf(fp_out, "{NULL, NULL, NULL},\n");
            fprintf(fp_out, "};\n");
            break;
        default:
            printf("[%s][%d]Trace line\n", __FUNCTION__, __LINE__);
            break;
    }
    printf("[%s][%d]ctype=%d\n",__FUNCTION__, __LINE__, ctype);
}

/*
**将数据模型文件翻译成C代码：
**1.节点回调函数映射表
**2.GET,SET函数实现
*/
int translate_core(const char *fileinput,const char *filec,const char *fileh,char *module_name){
    FILE *fp_in = NULL;
    FILE *fp_c = NULL;
    FILE *fp_h = NULL;

    fp_in = fopen(fileinput,"r");
    if(fp_in == NULL){
        return -1;
    }

    fp_c = fopen(filec, "w");
    if(fp_c == NULL){
        return -1;
    }

    fp_h = fopen(fileh, "w");
    if(fp_h == NULL){
        return -1;
    }

    /*Prepare the Functions File */

    /*Create the code of the file head.*/
    c_head_prepare(fp_c, module_name);
    /*Create the callback functions instance code.*/
    model_content_translate( fp_in, fp_c, C_CALLBACK_INST, module_name);
    
    /*Create the TY_USP_FUNC_TABLE array code.*/
    model_content_translate( fp_in, fp_c, C_CALLBACK_MAP_TABLE, module_name);
    //sleep(3);
    /*Create the TY_DYNAMIC_OBJ_TABLE array code.*/
    model_content_translate( fp_in, fp_c, C_DYNAMIC_OBJ_MAP_TABLE, module_name);
    /*Create the register functions code.*/
    c_tail_prepare(fp_c, module_name);

    /*Prepare the Header file*/
    /*Create the code of the file head.*/
    h_head_prepare(fp_h, module_name);
    /*Create the callback functions' decleration code.*/
    model_content_translate( fp_in, fp_h, C_CALLBACK_DECL,module_name);
    /*Create the tail of the header file.*/
    h_tail_prepare(fp_h, module_name);

    fclose(fp_in);
    fclose(fp_c);
    fclose(fp_h);

    return 0;
}

int main(int argc ,char ** argv){

    char *dot = NULL;
    char module_name[32] = {0};
    char file_input[128] = {0};
    char file_c[128] = {0};
    char file_h[128] = {0};

    if(argc<2){
        printf("please enter a filename to be translated.\n");
        return -1;
    }
    /*get the data-model filename which is going to be translated.*/
    strcpy(file_input,argv[1]);

    /*Get the module name base on the data-model file name.*/
    dot = strstr(file_input,".");
    if(dot != NULL){
        strncpy(module_name, file_input, dot -file_input);
    }
    /*the module name we want it to be lowercase.*/
    toLowerCase(module_name);

    printf("module name=[%s]\n", module_name);

    /*根据模块名，制作输出文件名*/
    sprintf(file_c, "ty_%s.c", module_name);
    sprintf(file_h, "ty_%s.h", module_name);

    printf("input[%s]\n C File[%s]\n Header File[%s]", file_input, file_c,file_h);

    /*将数据模型文件翻译成C代码*/
    translate_core(file_input, file_c, file_h, module_name);

    return 0;

}

