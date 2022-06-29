// Pipeline processor with superscalar features (fetching and decoding two instructions per cycle, reservation station, reorder buffer, store buffer)
// Akila Tharini Sivakumar, CS20B006

#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>
using namespace std;

int PC;
char A[2], B[2], LMD[2], ALUout[2];  //registers
int Reg[16];


int DATA_STALLS = 0;
int CONTROL_STALLS = 0;

//instructions of different types
int no_of_arith = 0;
int no_of_logic = 0;
int no_of_data = 0;
int no_of_cont = 0;
int no_of_halt = 0;


class Instruction
{
public:

    char I[4];
    string opcode;
    int src1;
    int src2;
    int dest;
    int fetch_order;
    int result;

    Instruction()
    {
        dest = 0;
        src1 = 0;
        src2 = 0;
        result = 0;
        opcode = "Not Set Yet";
    }

    Instruction(const Instruction& I2)
    {
        for (int j = 0; j < 4; j++)
            I[j] = I2.I[j];

        dest = I2.dest;
        src1 = I2.src1;
        src2 = I2.src2;
        opcode = I2.opcode;
        result = I2.result;
        fetch_order = I2.fetch_order;
    }
    
    void operator = (const Instruction &I2) 
    { 
         for (int j = 0; j < 4; j++)
            I[j] = I2.I[j];

        dest = I2.dest;
        src1 = I2.src1;
        src2 = I2.src2;
        opcode = I2.opcode;
        result = I2.result;
        fetch_order = I2.fetch_order;
    }

};

class Stage
{
public:
    int busy;
    int over;
    Instruction current;

    Stage()
    {
        busy = 0;
        over = 1;
    }
};

class Reservation_Station_Entry
{
public:

    int busy;
    Instruction I;
    int ready;

    Reservation_Station_Entry()
    {
        busy = 0;
        ready = 0;        
    }
};


class Store_Buff_Entry
{
public:
    Instruction SB_I;
    int occupied;

    Store_Buff_Entry()
    {
        occupied = 0;
    }
};

class RO_Buff_Entry
{
public:
    Instruction ROB_I;
    int occupied;

    RO_Buff_Entry()
    {
        occupied = 0;
    }
};

const int RES_STATION_SIZE = 12;
vector<Reservation_Station_Entry> Res_Station(RES_STATION_SIZE);  //Reservation Station

vector<RO_Buff_Entry> ROB(RES_STATION_SIZE);                      //Reorder buffer
vector<Store_Buff_Entry> Store_Buffer(RES_STATION_SIZE);          //Store Buffer

//functions to convert between different bases
int convert_to_int(char A[2])
{
    int num = 0;
    if (A[0] <= '7')
    {
        num += (A[0] - '0') * 16;
        if (A[1] <= '9')
        {
            num += (A[1] - '0');
        }
        else
        {
            num += A[1] - 87;
        }
    }
    else
    {
        if (A[0] <= '9')
        {
            num += (A[0] - '0') * 16;
        }
        else
        {
            num += (A[0] - 87) * 16;
        }

        if (A[1] <= '9')
        {
            num += (A[1] - '0');
        }
        else
        {
            num += A[1] - 87;
        }

        num = num - 128;
        num = (-128 + num);

    }

    return num;
}

int convert_to_int_1(char A)
{
    int num = 0;
    if (A <= '9')
    {
        num = (A - '0');
    }
    else
    {
        num = (A - 87);
    }


    return num;
}

void convert_2s_comp_hex(char res[2], int n)
{
    if (n >= 0)
    {
        ; //already taken care of
    }
    else
    {
        int x = 128 + n;
        int m = x % 16;


        if (m <= 9)
            res[1] = '0' + m;
        else
            res[1] = 'a' + m - 10;

        int s = 8;
        x = x / 16;
        s += x;


        if (s <= 9)
            res[0] = '0' + s;
        else
            res[0] = 'a' + s - 10;
    }
}

char convert_to_hex(int a)
{
    if (a <= 9)
        return a + 48;
    else
        return a + 87;
}

//compute using ALU
int ALU(int op1, int op2, string code)
{
    if (code == "ADD")
        return op1 + op2;
    else if (code == "SUB")
        return op1 - op2;
    else if (code == "MUL")
        return op1 * op2;
    else if (code == "INC")
        return op1 + 1;
    else if (code == "AND")
        return op1 & op2;
    else if (code == "OR")
        return op1 | op2;
    else if (code == "NOT")
        return ~op1;
    else if (code == "XOR")
        return op1 ^ op2;
    else
        return 0;
}

//functions to determine offset and index to access the data cache
int findOffset(char A[2])
{
    int decimal = convert_to_int(A);
    int offset = 0;
    offset += decimal % 2;
    decimal /= 2;
    offset += (decimal % 2) * 2;
    return offset;
}

int findIndex(char A[2])
{
    int decimal = convert_to_int(A);
    int index = decimal >> 2;
    return index;
}

//assign opcodes
string set_opcode(int n)
{

    if (n == 0)
    {
        no_of_arith++;
        return "ADD";
    }
    else if (n == 1)
    {
        no_of_arith++;
        return "SUB";
    }
    else if (n == 2)
    {
        no_of_arith++;
        return "MUL";
    }
    else if (n == 3)
    {
        no_of_arith++;
        return "INC";
    }
    else if (n == 4)
    {
        no_of_logic++;
        return "AND";
    }
    else if (n == 5)
    {
        no_of_logic++;
        return "OR";
    }
    else if (n == 6)
    {
        no_of_logic++;
        return "NOT";
    }
    else if (n == 7)
    {
        no_of_logic++;
        return "XOR";
    }
    else if (n == 8)
    {
        no_of_data++;
        return "LOAD";
    }
    else if (n == 9)
    {
        no_of_data++;
        return "STORE";
    }
    else if (n == 10)
    {
        no_of_cont++;
        return "JMP";
    }
    else if (n == 11)
    {
        no_of_cont++;
        return "BEQZ";
    }
    else if (n == 15)
    {
        no_of_halt++;
        return "HLT";
    }
    else
        return "ERROR";
}

Instruction decode(char inst[4])
{

    int op_c = convert_to_int_1(inst[0]);
    Instruction D;

    D.opcode = set_opcode(op_c);
    D.src1 = convert_to_int_1(inst[2]);
    D.src2 = convert_to_int_1(inst[3]);
    D.dest = convert_to_int_1(inst[1]);

    if (D.opcode == "INC")
    {
        D.src1 = convert_to_int_1(inst[1]);
        D.src2 = convert_to_int_1(inst[1]);
        D.dest = convert_to_int_1(inst[1]);
    }
    
    if (D.opcode == "LOAD")
    {
        D.src1 = convert_to_int_1(inst[2]);
        D.src2 = convert_to_int_1(inst[2]);
        D.dest = convert_to_int_1(inst[1]);
    }
    
    if (D.opcode == "STORE")
    {
        D.src1 = convert_to_int_1(inst[1]);
        D.src2 = convert_to_int_1(inst[2]);
        D.dest = -1;
    }

    if (D.opcode == "NOT")
    {
        D.src1 = convert_to_int_1(inst[2]);
        D.src2 = convert_to_int_1(inst[2]);
        D.dest = convert_to_int_1(inst[1]);
    }

    if (D.opcode == "BEQZ")
    {
        D.src1 = convert_to_int_1(inst[1]);
        D.src2 = convert_to_int_1(inst[1]);
        D.dest = -1;
    }

    for (int j = 0; j < 4; j++)
    {
        (D.I)[j] = inst[j];
    }

    return D;
}

int isNotEmpty_SB()
{
    for (int p = 0; p < Store_Buffer.size(); p++)
    {
        if (Store_Buffer[p].occupied == 1)
            return 1;
    }
    return 0;
}

int isNotEmpty_ROB()
{
    for (int p = 0; p < ROB.size(); p++)
    {
        if (ROB[p].occupied == 1)
        {
            //cout<<"ROB index "<<p<<" occupied"<<endl;
            return 1;
        }
    }
    return 0;
}

int isNotEmpty_RS()
{
    for (int p = 0; p < Res_Station.size(); p++)
    {
        if (Res_Station[p].busy == 1)
            return 1;
    }
    return 0;
}



int curr_time;   //total no of cycles
Stage IF, IF2, ID, ID2, EX, EX2, MEM, BR, WB;   //5 stages of the pipeline

int main()
{
    ifstream ic;
    fstream dc, rf, stats;

    //input files
    ic.open("ICache.txt");
    dc.open("DCache.txt", ios::out | ios::in);
    rf.open("RF.txt", ios::out | ios::in);

    int no_of_instructions, no_of_inst_from_fetch = 0;
    ic.seekg(0, ios::end);
    no_of_instructions = ic.tellg() / 6;
    ic.seekg(0, ios::beg);

    for (int i = 0; i < 16; i++)
        Reg[i] = 1;   //correct data, ready to supply

    Instruction inst;
    int dont_read_anymore = 0;
    int next_inst_id = 1;

    PC = 0;
    int cond;
    int INST_DECODED = 0;
    int INST_DECODED2 = 0;
    int waiting_for_branch = 0;

    do
    {
        //Store Buff STAGE
        if(isNotEmpty_SB)
        {
            for (int k = 0; k < Store_Buffer.size(); k++)
            {
                if(Store_Buffer[k].occupied==1)
                {
                    inst=Store_Buffer[k].SB_I;
                    //cout<<"reg whose contents to be stored: "<<inst.src1<<endl;
                    
                    rf.seekg(inst.src2 * 3, ios::beg);
                    rf.read(A, 2);

                    int R2 = convert_to_int(A);

                    char offs[2];
                    offs[1] = inst.I[3];

                    if (inst.I[3] < '8')
                        offs[0] = '0';
                    else
                        offs[0] = 'f';

                    int X = convert_to_int(offs);

                    int address = ALU(R2, X, "ADD");
                            
                    char ad[2];                    
                    ad[0] = convert_to_hex(address / 16);
                    ad[1] = convert_to_hex(address % 16);

                    int index = findIndex(ad);
                    int offset = findOffset(ad);

                    //cout<<"index = "<<index<<" offset = "<<offset<<endl;
                                    
                    rf.seekg(inst.src1 * 3, ios::beg);
                    rf.read(LMD, 2);              
                                        
                    dc.seekg(index * 12 + offset * 3, ios::beg);
                    dc.write(LMD, 2);
                    
                    //cout<<"written to dc "<<LMD[0]<<LMD[1]<<endl;
                    inst.result = convert_to_int(LMD);
                    
                    //cout<<endl<<endl<<inst.result<<" written in "<<"index "<<index<<" offset "<<offset<<endl<<endl;
                    
                    Store_Buffer[k].occupied=0;
                    
                    break;                
                }            
            }        
        
        }
                
        //ROB Stage
        if (isNotEmpty_ROB())
        {
            //cout<<"ROB not empty"<<endl;
            for (int k = 0; k < ROB.size(); k++)
            {
                if (ROB[k].ROB_I.fetch_order == next_inst_id)
                {
                    next_inst_id++;
                    //cout<<" next id is now "<<next_inst_id<<endl;
                    
                    ROB[k].occupied = 0;
                    inst=ROB[k].ROB_I;

                    if (ROB[k].ROB_I.opcode == "JMP" || ROB[k].ROB_I.opcode == "BEQZ")
                    {
                        //cout << ROB[k].ROB_I.opcode << " out of ROB - completed" << endl;
                    }
                    else if (ROB[k].ROB_I.opcode == "LOAD")
                    {
                        char res[2];
                        if (inst.result >= 0)
                        {
                            res[0] = convert_to_hex(inst.result / 16);
                            res[1] = convert_to_hex(inst.result % 16);
                        }
                        else
                        {
                            convert_2s_comp_hex(res, inst.result);
                        }
                            
                        rf.seekg((inst.dest) * 3, ios::beg);
                        rf.write(res, 2);
                        
                        //cout<<res<<" from res written into "<<inst.dest<<" "<<endl;                        

                        Reg[inst.dest] = 1;
                        
                        //cout<<inst.dest<<" set to ready"<<endl;
                        //cout << ROB[k].ROB_I.opcode << " out of ROB - completed" << endl;
                    }
                    else if (ROB[k].ROB_I.opcode == "STORE")
                    {
                        //cout << ROB[k].ROB_I.opcode << " out of ROB - completed, semt to store buffer" << endl;
                        
                        for (int p = 0; p < Store_Buffer.size(); p++)
               	 {
                   	     if (Store_Buffer[p].occupied == 0)
                    	     {
                                                                           
                       	Store_Buffer[p].occupied = 1;
                       	Store_Buffer[p].SB_I = inst;
                        
                        	//cout<<endl<<endl<<ROB[k].ROB_I.opcode<<" inst sent to SB index "<<k<<" fetch order is "<<ROB[k].ROB_I.fetch_order<<endl;
                        	
                        	ROB[k].occupied=0;
                 		//cout<<"ROB index "<<k<<" freed"<<endl;
                    
                        	break;
                        
                   	      }                    
                    	      //cout<<"index "<<k<<" in Store Buffer not free to send, occupied by "<<Store_Buffer[k].SB_I.I<<endl;
               	 }                        
                    }
                    else
                    {
                        //cout<<ROB[k].ROB_I.opcode<<" arithmetic inst processed in ROB"<<endl;
                        //cout<<endl<<endl<<"result is "<<ROB[k].ROB_I.result<<endl<<endl;                        
                        
                        rf.seekg((inst.dest) * 3, ios::beg);

                        char res[2];
                        if (inst.result >= 0)
                        {
                            res[0] = convert_to_hex(inst.result / 16);
                            res[1] = convert_to_hex(inst.result % 16);
                        }
                        else
                        {
                            convert_2s_comp_hex(res, inst.result);
                        }

                        rf.write(res, 2);

                        Reg[inst.dest] = 1;
                        //cout<<inst.dest<<" set to ready"<<endl;                        
                        //cout << ROB[k].ROB_I.opcode << " out of ROB - completed" << endl;
                    }
                    ROB[k].occupied=0;
                    //cout<<"ROB index "<<k<<" freed"<<endl;
                }
            }
        }
        
        for(int k=0;k<RES_STATION_SIZE;k++)
        {
           if(Res_Station[k].busy==1)
           {
             int operand1 = Res_Station[k].I.src1;
             int operand2 = Res_Station[k].I.src2;
             if(Reg[operand1]==1&&Reg[operand2]==1)
               Res_Station[k].ready=1;
           }
        }

        //BR Stage
        if (BR.busy)
        {
            inst = BR.current;
            //cout<<"BR stage has "<<inst.I<<endl;
            
            if (BR.over != 1)
            {
                if(inst.opcode=="JMP")
                {
                   char t[2];
                   t[0]=inst.I[1];
                   t[1]=inst.I[2];
                   int L2=convert_to_int(t);
                      
                   int addr = PC + (L2*6); //L2 has to be sign extended offset
                   PC=addr;
                   
                   //cout<<"JMP resolved, PC set to "<<PC<<endl;
                   
                }                   
                else if(inst.opcode=="BEQZ")
                {
                    rf.seekg(inst.src1 * 3, ios::beg);
                    rf.read(A, 2);
                    
                        if(A[0]=='0'&&A[1]=='0')
                        {
                           
                           char t[2];
                           t[0]=inst.I[2];
                           t[1]=inst.I[3];
                           int L2=convert_to_int(t);
                           
                           int addr = PC + (L2*6); //L2 has to be sign extended offset
                           
                           PC=addr;
                           
                           //cout<<"BEQZ resolved, PC set to "<<PC<<endl;
                           
                        }
                        //else
                        // cout<<"BEQZ resolved, PC remains "<<PC<<endl;
                }         
                                
                waiting_for_branch = 0;
                //cout<<"not waiting for branch anymore"<<endl;

                for (int k = 0; k < ROB.size(); k++)
                {
                    if (ROB[k].occupied == 0)
                    {
                        ROB[k].occupied = 1;
                        ROB[k].ROB_I = inst;
                        
                        //cout<<endl<<endl<<ROB[k].ROB_I.opcode<<" inst sent to ROB index "<<k<<" fetch order is "<<ROB[k].ROB_I.fetch_order<<endl;
                        
                        BR.busy = 0;
                        BR.over = 0;
                        break;
                    }
                }
                
            }
        }

        //Memory Stage
        if (MEM.busy)
        {
            inst = MEM.current;
            //cout<<"MEM stage has "<<inst.I<<endl;
            
            if (MEM.over != 1)
            {
                if (inst.opcode == "LOAD")
                {
                    rf.seekg(inst.src1 * 3, ios::beg);
                    rf.read(A, 2);

                    int R2 = convert_to_int(A);

                    Reg[inst.dest] = 0;

                    char offs[2];
                    offs[1] = inst.I[3];

                    if (inst.I[3] < '8')
                        offs[0] = '0';
                    else
                        offs[0] = 'f';

                    int X = convert_to_int(offs);


                    int address = ALU(R2, X, "ADD");                    
                    
                    char ad[2];
                    ad[0] = convert_to_hex(address / 16);
                    ad[1] = convert_to_hex(address % 16);

                    int index = findIndex(ad);
                    int offset = findOffset(ad);

                    //cout<<"index = "<<index<<" offset = "<<offset<<endl;
                                        
                    dc.seekg(index * 12 + offset * 3, ios::beg);
                    dc.read(LMD, 2);

                    //cout<<"read from dc "<<LMD[0]<<LMD[1]<<endl;
                    inst.result = convert_to_int(LMD);
                    
                    //cout<<inst.result<<" read in MEM"<<endl;
                    
                    if (isNotEmpty_ROB)
                    {                     
                        for (int k = 0; k < ROB.size(); k++)
                        {
                            if (ROB[k].occupied == 0)
                            {
                                ROB[k].ROB_I = inst;
                                ROB[k].occupied = 1;
                                MEM.busy = 0;
                                MEM.over = 0;
                                
                                break;
                            }
                        }
                    }
            }
            }
        }

        //EXECUTE STAGE
        if (EX.busy)
        {
            inst = EX.current;            
            //cout<<"EX stage has "<<inst.I<<" fetch order = "<<inst.fetch_order<<endl;

            if (EX.over != 1)
            {
                rf.seekg(inst.src1 * 3, ios::beg);
                rf.read(A, 2);

                rf.seekg(inst.src2 * 3, ios::beg);
                rf.read(B, 2);

                int a = convert_to_int(A);
                int b = convert_to_int(B);

                int res = ALU(a, b, inst.opcode);

                //cout<<"a= "<<a<<" b= "<<b<<" res= "<<res<<endl;
                
                inst.result = res;
                EX.over = 1;

            }
            
            for (int k = 0; k < ROB.size(); k++)
                {
                    if (ROB[k].occupied == 0)
                    {
                                                                           
                        ROB[k].occupied = 1;
                        ROB[k].ROB_I = inst;
                        
                        //cout<<endl<<endl<<ROB[k].ROB_I.opcode<<" arithmetic inst sent to ROB index "<<k<<" fetch order is "<<ROB[k].ROB_I.fetch_order<<endl;
                        
                        EX.busy = 0;
                        EX.over = 0;
                        break;
                        
                    }
                    
                    //cout<<"index "<<k<<" in ROB not free to send, occupied by "<<ROB[k].ROB_I.I<<endl;
                }
        }
        
        //EXECUTE STAGE 2
        if (EX2.busy)
        {
            inst = EX2.current;
            //cout<<"EX2 stage has "<<inst.I<<" fetch order = "<<inst.fetch_order<<endl;

            if (EX2.over != 1)
            {
                rf.seekg(inst.src1 * 3, ios::beg);
                rf.read(A, 2);

                rf.seekg(inst.src2 * 3, ios::beg);
                rf.read(B, 2);

                int a = convert_to_int(A);
                int b = convert_to_int(B);

                int res = ALU(a, b, inst.opcode);

                //cout<<"a= "<<a<<" b= "<<b<<" res= "<<res<<endl;
                
                inst.result = res;
                EX2.over = 1;

            }
            
            for (int k = 0; k < ROB.size(); k++)
                {
                    if (ROB[k].occupied == 0)
                    {
                                                                           
                        ROB[k].occupied = 1;
                        ROB[k].ROB_I = inst;
                        
                        //cout<<endl<<endl<<ROB[k].ROB_I.opcode<<" arithmetic inst sent to ROB index "<<k<<" fetch order is "<<ROB[k].ROB_I.fetch_order<<endl;
                        
                        EX2.busy = 0;
                        EX2.over = 0;
                        break;
                        
                    }                    
                    //cout<<"index "<<k<<" in ROB not free to send, occupied by "<<ROB[k].ROB_I.I<<endl;
                }
        }
        
        //INSTRUCTION DECODE STAGE
        if (ID.busy)
        {
            inst = ID.current;
            //cout<<"ID stage has "<<inst.I<<" "<<inst.opcode<<" dest "<<inst.dest<<" sources "<<inst.src1<<" "<<inst.src2<<" with fetch order "<<ID.current.fetch_order<<endl;

            if (ID.over != 1)
            {
                if (INST_DECODED != 1)
                {
                    ID.current = decode(inst.I);
                    ID.current.fetch_order=inst.fetch_order;
                    inst=ID.current;
                     
                    //cout<<"ID stage has "<<inst.I<<" "<<inst.opcode<<" dest "<<inst.dest<<" sources "<<inst.src1<<" "<<inst.src2<<" with fetch order "<<inst.fetch_order<<endl;

                    if (ID.current.opcode == "HLT")  //don't read any more instructions
                    {
                        dont_read_anymore = 1;
                        
                        //cout<<"Dont read anymore set"<<endl;
                        
                        no_of_inst_from_fetch--;
                        ID2.busy=0; 
                        IF2.busy=0; 
                        
                        ID.busy = 0;
                        IF.busy = 0;
                        continue;
                    }

                    if (inst.opcode == "JMP" || inst.opcode == "BEQZ")
                    {
                        waiting_for_branch = 1;
                        
                        PC=PC-6; 
                        no_of_inst_from_fetch--;
                        ID2.busy=0;  
                        ID2.over=0; 
                        //cout<<"branch in ID, id2 set to not busy, not over"<<endl<<"PC set to "<<PC<<endl;

                    }
                    
                    if (inst.opcode == "LOAD")
                    {
                       ;
                    }
                    
                    if (inst.opcode == "STORE")
                    {
                       //cout<<"STORE instruction decoded in ID"<<endl;
                    }
                    
                    if(inst.opcode!="JMP"&&inst.opcode!="BEQZ"&&inst.opcode!="LOAD"&&inst.opcode!="STORE"&&inst.opcode!="HLT")
                    {
                       ;
                    }

                    INST_DECODED = 1;
                    inst = ID.current;
                    inst.fetch_order=ID.current.fetch_order;

                }

                ID.over = 1;

                for (int k = 0; k < RES_STATION_SIZE; k++)
                {
                    if (Res_Station[k].busy != 1)
                    {
                        Res_Station[k].I = inst;
                        
                        //cout<<"inst in id stage "<<inst.I<<" fetch order "<<inst.fetch_order<<endl;
                        //cout<<inst.I<<" in RES index "<<k<<endl;
                        
                        Res_Station[k].busy = 1;
                        if(inst.opcode=="JMP")
                            Res_Station[k].ready = 1;
                        else if (Reg[inst.src1] == 1&& Reg[inst.src2]==1)
                            Res_Station[k].ready = 1;
                        
                        if(inst.opcode!="JMP"&&inst.opcode!="BEQZ"&&inst.opcode!="STORE"&&inst.opcode!="HLT")
                        {
                           Reg[inst.dest] = 0;
                           //cout<<inst.dest<<" set to not ready"<<endl;
                        }                       
                        
                        ID.busy = 0;
                        ID.over=0;
                        INST_DECODED=0;
                        break;
                    }
                }

            }


        }

         //INSTRUCTION DECODE STAGE 2
        if (ID2.busy)
        {
            inst = ID2.current;
            //cout<<"ID2 stage has "<<inst.I<<" with fetch order from current "<<ID.current.fetch_order<<endl;


            if (ID2.over != 1)
            {
                if (INST_DECODED2 != 1)
                {
                    ID2.current = decode(inst.I);
                    ID2.current.fetch_order=inst.fetch_order;
                    inst=ID2.current;
                    
                    //cout<<"ID2 stage has "<<inst.I<<" "<<inst.opcode<<" dest "<<inst.dest<<" sources "<<inst.src1<<" "<<inst.src2<<" with fetch order "<<inst.fetch_order<<endl;

                    if (ID2.current.opcode == "HLT")  //don't read any more instructions
                    {
                        dont_read_anymore = 1;
                        
                        //cout<<"Dont read anymore set"<<endl;
                        
                        ID2.busy = 0;
                        IF2.busy = 0;
                        continue;
                    }

                    if (inst.opcode == "JMP" || inst.opcode == "BEQZ")
                    {
                        waiting_for_branch = 1;
                    }
                    
                    if (inst.opcode == "LOAD")
                    {
                       ;
                    }
                    
                    if (inst.opcode == "STORE")
                    {
                       //cout<<"STORE instruction decoded in ID2 "<<endl;
                    }
                    
                    if(inst.opcode!="JMP"&&inst.opcode!="BEQZ"&&inst.opcode!="STORE"&&inst.opcode!="HLT")
                    {
                       ;
                    }

                    INST_DECODED2 = 1;
                    inst = ID2.current;
                    inst.fetch_order=ID2.current.fetch_order;

                }

                ID2.over = 1;

                for (int k = 0; k < RES_STATION_SIZE; k++)
                {
                    if (Res_Station[k].busy != 1)
                    {
                        Res_Station[k].I = inst;
                        
                        //cout<<"inst in id2 stage "<<inst.I<<" fetch order "<<inst.fetch_order<<endl;
                        //cout<<inst.I<<" in RES index "<<k<<endl;
                        
                        Res_Station[k].busy = 1;
                        Res_Station[k].ready = 0;                        
                        if(inst.opcode=="JMP")
                            Res_Station[k].ready = 1;
                        else if (Reg[inst.src1] == 1&& Reg[inst.src2]==1)
                            Res_Station[k].ready = 1;
                            
                        if(inst.opcode!="JMP"&&inst.opcode!="BEQZ"&&inst.opcode!="STORE"&&inst.opcode!="HLT")
                        {
                           Reg[inst.dest] = 0;
                           //cout<<inst.dest<<" set to not ready"<<endl;
                        }     
                            
                        ID2.busy = 0;
                        ID2.over=0;
                        INST_DECODED2=0;
                        break;
                    }
                }

            }


        }
        
        //DISPACTCH STAGE
        int any_stall=0;
        for (int k = 0; k < RES_STATION_SIZE; k++)
        {
            if (Res_Station[k].busy == 1)
            {
                if (Res_Station[k].ready == 1)
                {
                    Instruction I_curr;
                    I_curr = Res_Station[k].I;
                    
                    //cout<<"I_curr in despatching stage "<<I_curr.I<<" fetch order "<<I_curr.fetch_order<<endl;
                        
                    if (I_curr.opcode == "JMP")
                    {
                                                
                        if (!BR.busy)
                        {
                            //cout<<"JMP sent to br"<<endl;                        
                            BR.busy = 1;
                            BR.over = 0;
                            BR.current = I_curr;
                            Res_Station[k].ready = 0;
                            Res_Station[k].busy = 0;
                        }                        
                    }                
                    else if (I_curr.opcode == "BEQZ")
                    {
                        if (!BR.busy)
                        {
                            //cout<<"Branch sent to br"<<endl;
                        
                            BR.busy = 1;
                            BR.over = 0;
                            BR.current = I_curr;
                            Res_Station[k].ready = 0;
                            Res_Station[k].busy = 0;
                        }
                    }
                    else if (I_curr.opcode == "LOAD")
                    {
                        if (!MEM.busy)
                        {
                            //cout<<"Load sent to mem"<<endl;
                            
                            MEM.busy = 1;
                            MEM.over = 0;
                            MEM.current = I_curr;
                            Res_Station[k].ready = 0;
                            Res_Station[k].busy = 0;
                        }
                        //else
                           //cout<<"waiting for mem to become avilable"<<endl;  
                    }
                    else if (I_curr.opcode == "STORE")
                    {
                        for (int p = 0; p < ROB.size(); p++)
                        {
                            if (ROB[p].occupied == 0)
                            {
                                //cout<<"Store sent to ROB index "<<k<<endl;
                            
                                ROB[p].occupied = 1;
                                ROB[p].ROB_I = I_curr;
                                Res_Station[k].ready = 0;
                                Res_Station[k].busy = 0;
                                break;
                            }
                        }

                    }
                    else 
                    {
                        if (!EX.busy)
                        {
                        
                            //cout<<I_curr.opcode<<" arithmetic inst sent to EX"<<endl;                            
                            EX.busy = 1;
                            EX.over = 0;
                            EX.current = I_curr;
                            Res_Station[k].ready = 0;
                            Res_Station[k].busy = 0;
                        }
                        else if (!EX2.busy)
                        {                        
                            //cout<<I_curr.opcode<<" arithmetic inst sent to EX2"<<endl;                            
                            EX2.busy = 1;
                            EX2.over = 0;
                            EX2.current = I_curr;
                            Res_Station[k].ready = 0;
                            Res_Station[k].busy = 0;
                        }
                        //else
                        // cout<<"EX and EX 2 busy"<<endl;
                    }
                }
                else
                {
                    any_stall=1;
                    //cout<<"trying to dispatch "<<Res_Station[k].I.I<<" "<<Res_Station[k].I.src1<<" "<<Reg[Res_Station[k].I.src1]<<" "<<Res_Station[k].I.src2<<" "<<Reg[Res_Station[k].I.src2]<<endl;
                }
                
            }
        }
        if(any_stall)
        {
           DATA_STALLS++;     ///Data Stall whenever an instruction is in the reservation station but operands are not yet ready
        }
        
        //INSTRUCTION FETCH STAGE
        if (!IF.busy && !dont_read_anymore)
        {
            if (waiting_for_branch)
            {
                CONTROL_STALLS++;
                //cout<<"no instruction fetched in IF, waiting for branch"<<endl;
            }
            else
            {
                char temp_i[3], dummy[2];
                ic.seekg(PC, ios::beg);
                ic.read(temp_i, 2);
                inst.I[0] = temp_i[0];
                inst.I[1] = temp_i[1];

                ic.seekg(1, ios::cur);
                ic.read(temp_i, 2);
                inst.I[2] = temp_i[0];
                inst.I[3] = temp_i[1];

                ic.read(dummy, 2);

                no_of_inst_from_fetch++;
                inst.fetch_order=no_of_inst_from_fetch;      
                
                IF.current = inst;
                
                //cout<<"Inst read "<<inst.I<<" with fetch order = "<<inst.fetch_order<<endl;

                PC = PC + 6;

                IF.over = 1;
                IF.busy = 1;

                if (!ID.busy)
                {
                    ID.current = inst;
                    ID.busy = 1;
                    ID.over = 0;
                    IF.busy = 0;
                }

            }
        }
        else if (IF.busy && !dont_read_anymore)
        {
            //cout<<"IF stage has "<<inst.I<<endl;
            if (!ID.busy)
            {
                ID.current = IF.current;
                ID.busy = 1;
                ID.over = 0;
                IF.busy = 0;
            }
        }
        
        
        //INSTRUCTION FETCH STAGE 2
        if (!IF2.busy && !dont_read_anymore)
        {
            if (waiting_for_branch)
            {
                CONTROL_STALLS++;
                //cout<<"no instruction fetched in IF2, waiting for branch"<<endl;
            }
            else
            {
                char temp_i[3], dummy[2];
                ic.seekg(PC, ios::beg);
                ic.read(temp_i, 2);
                inst.I[0] = temp_i[0];
                inst.I[1] = temp_i[1];

                ic.seekg(1, ios::cur);
                ic.read(temp_i, 2);
                inst.I[2] = temp_i[0];
                inst.I[3] = temp_i[1];

                ic.read(dummy, 2);

                no_of_inst_from_fetch++;
                inst.fetch_order=no_of_inst_from_fetch;      
                
                IF2.current = inst;
                
                //cout<<"IF2 Inst read "<<inst.I<<" with fetch order = "<<inst.fetch_order<<endl;

                PC = PC + 6;

                IF2.over = 1;
                IF2.busy = 1;

                if (!ID2.busy)
                {
                    ID2.current = inst;
                    ID2.busy = 1;
                    ID2.over = 0;
                    IF2.busy = 0;
                }
            }
        }
        else if (IF2.busy && !dont_read_anymore)
        {
            //cout<<"IF2 stage has "<<inst.I<<endl;
            if (!ID2.busy)
            {
                ID2.current = IF2.current;
                ID2.busy = 1;
                ID2.over = 0;
                IF2.busy = 0;
            }
        }

        curr_time++;   //update time counter
        
        //cout<<endl<<endl<<"Cycle "<<curr_time<<" over"<<endl<<endl;

    } while ((WB.busy || MEM.busy || EX.busy ||BR.busy || ID.busy || IF.busy||IF2.busy||ID2.busy||EX2.busy)|| isNotEmpty_RS()|| isNotEmpty_ROB()|| isNotEmpty_SB()); 
        
    ofstream foutput;
    foutput.open("Output.txt");

    foutput<<"Total number of instructions: "<<no_of_inst_from_fetch<<"\n";
    foutput<<"Number of instructions in each class"<<"\n";
    foutput<<"Arithmetic instructions: "<<no_of_arith<<"\n";
    foutput<<"Logical instructions: "<<no_of_logic<<"\n";
    foutput<<"Data instructions: "<<no_of_data<<"\n";
    foutput<<"Control instructions: "<<no_of_cont<<"\n";
    foutput<<"Halt instructions: "<<no_of_halt<<"\n";
    
    //foutput << "Total time = " << curr_time << endl;
    
    foutput<<"Cycles Per Instruction: "<<(float) curr_time/no_of_inst_from_fetch<<"\n";
    foutput<<"Total number of stalls: "<<DATA_STALLS+CONTROL_STALLS<<"\n";
    foutput<<"Data stalls (RAW): "<<DATA_STALLS<<"\n";
    foutput<<"Control stalls: "<<CONTROL_STALLS<<"\n";  

}