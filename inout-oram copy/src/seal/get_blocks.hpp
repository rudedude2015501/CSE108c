#pragma once
//Goal is to return a dataset of keyword and the doc_ids, which will be the data of the key
//although not entirly path oram, this will be helpful
//use blocks.hpp
//each block is uint32 and data uint_8 * 4

#include <cstdint>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <string.h>
#include "oram/common/block.hpp"
#include <iostream>
#include <bitset>
std::vector<std::pair<std::string, std::vector<uint8_t>>> get_blocks(char *directory, uint32_t blocknumber)
{
    //Open the csv file
    std::ifstream fout(directory);
    if (!fout){
        //throw error
    }
    std::string buff;
    getline(fout,buff);//gets the colum values
    getline(fout, buff);//skipsthe line
    std::vector<std::pair<std::string, std::vector<uint8_t>>> output;
    for(uint32_t i=0; i<blocknumber; i++)
    {
        buff.clear();
        getline(fout,buff);
        int ind = buff.find(','); //findsthe first commaf for us to parse the key
        std::string key = buff.substr(0,ind);
        
        buff = buff.substr(5, buff.length()-5);
        std::vector<uint8_t> data(buff.begin(),buff.end());
        output.push_back({key,data});
    }
    return output;
}