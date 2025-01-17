/*基于隐马尔可夫模型，利用维特比算法进行中文分词、标注*/
#include<iostream>
#include<string>
#include<fstream>
#include<sstream>
#include<cmath>
#include<vector>
#include<stack>
using namespace std;

static char status[5] = { 'S','B','m','M','E' };
vector<string> dictionary;//字典
int MaxNum = 4;//逆向最大匹配取词个数
vector<string> wordTbl;//单词表，记录所有语料库中出现的单个字，不重复。
int numOfWord = 0;//非重复汉字个数。
double dataOfWord[10000][6] = { 0 };//记录相应汉字的各种数据，如出现频率、Begin、Middle、End、Single
double TransProbMatrix[5][5] = { 0 };//状态转移概率[SBME]*[SBME]
double initStatus[5] = { 0 };//状态初始向量
vector<string> inputSentence;//需要进行分词处理的句子中的汉字，重复

static string POSTbl[] = { "Ag","a","ad","an","b","c","Dg","d","e","f","g","h","i","j","k",
							"l","m","Ng","n","nr","ns","nt","nz","o","p","q","r","s","Tg",
							"t","u","Vg","v","vd","vn","w","x","y","z","un" };//词性表
vector<string> wordOfPOSTbl;//用于记录所有语料库中出现过的词汇，不重复
double dataOfPOSTbl[100000][41] = { 0 };//记录相应词汇的各种词性数据
double POSTransProbMatrix[40][40] = { 0 };//词性状态转移矩阵
double POSInitStatus[40] = { 0 };//词性初始向量
double tagProb[41] = { 0 };//记录各种词性的出现频率,初始赋值为1
vector<string> DataOfSegmentation;//算法判断的最优分词结果，分词后的词语存储
vector<string> F_DataOfSegmentation;//正向分词后的词语存储
vector<string> R_DataOfSegmentation;//逆向分词后的词语存储
vector<string> HMM_DataOfSegmentation;//HMM分词结果

void DictFileToData();
void DealWithCorpus();
bool IsRepeatOrNot(string word);
void WritePosition(int mark);
void WritePosition(int mark, int position);
int PositionOfSubword(string subword);
void UpDateMatrix(int prveious, int mark);
void FloatToIn();
void GetInitStatus(int previous, int mark);
void SentenceToData();
double** FindEmitProbMatrix();
void ShowEmitProbMatrix(double **matrix);
void ShowPosEmitProbMatrix(double **matrix);
void GetProbOderOfStatus(double**martrix);
double FindMax(double*);
int FindMaxNum(double*);
void WriteDataToFile();
void ReadDataFromFile();
int ReverseMaximumMatching();
int ForwardMaximunMatching();
void FindMaxMatchResult(int result1, int result2, int result3);
void DealWithCorpusForPOS();
int FindPosOfWord(string pos);
void UpdateInitPosStatus(int previous, int current);
void UpdatePosTransProbMatrix(int previous, int current);
int FindWordInPosTbl(string word);
double**FindPosEmitProbMatrix();
void GetPosProbOderOfStatus(double**martrix);
double posFindMax(double *prob);
int posFindMaxNum(double *prob);
int EffectOfHMMSeg();

int main()
{
	double **EmitProbMatrix;
	double **PosEmitProbMatrix;
	int i = 0;
	cout << "(0:dealwith corpus  1:read data from file)：";
	cin >> i;
	if (i == 0)
	{
		DealWithCorpusForPOS();
		DealWithCorpus();
		WriteDataToFile();
		DictFileToData();
		FloatToIn();
	}
	else if(i==1)
	{
		ReadDataFromFile();
		DictFileToData();
		FloatToIn();
	}
	else
	{
		cout << "please input 0 or 1!";
	}
	while (true)
	{
		int resultOfF_ = 0;
		int resultOfR_ = 0;
		int resultOfHMM = 0;
		SentenceToData();
		cout << "正向最大匹配分词：" << endl;
		resultOfF_ = ForwardMaximunMatching();

		cout << "逆向最大匹配分词：" << endl;
		resultOfR_=ReverseMaximumMatching();

		cout << endl << "基于HMM的维特比分词：";
		EmitProbMatrix = FindEmitProbMatrix();
		//		ShowEmitProbMatrix(EmitProbMatrix);
		GetProbOderOfStatus(EmitProbMatrix);
		resultOfHMM = EffectOfHMMSeg();

		FindMaxMatchResult(resultOfF_, resultOfR_,resultOfHMM);
		PosEmitProbMatrix = FindPosEmitProbMatrix();
	//	ShowPosEmitProbMatrix(PosEmitProbMatrix);
		cout << endl << "词性标注结果：" << endl;
		GetPosProbOderOfStatus(PosEmitProbMatrix);
		F_DataOfSegmentation.clear();
		R_DataOfSegmentation.clear();
		HMM_DataOfSegmentation.clear();
		DataOfSegmentation.clear();
		inputSentence.clear();
		DataOfSegmentation.clear();
	}
	return 0;
}
/*找到正、逆最大匹配中最好的结果*/
void FindMaxMatchResult(int result1, int result2, int result3)
{
	if (result1 < result2&&result1 < result3)
	{
		DataOfSegmentation = F_DataOfSegmentation;
		cout << "选择正向最大匹配分词结果！" << endl;
	}
	else if (result3 < result1&&result3 < result2)
	{
		DataOfSegmentation = HMM_DataOfSegmentation;
		cout << "选择HMM分词结果！" << endl;
	}
	else
	{
		DataOfSegmentation = R_DataOfSegmentation;
		cout << "选择逆向最大匹配分词结果！" << endl;
	}
	/*
	if (result1<result2)
	{
		DataOfSegmentation = F_DataOfSegmentation;
		cout << "选择正向最大匹配结果！" << endl;
	}
	else
	{
		DataOfSegmentation = R_DataOfSegmentation;
		cout << "选择逆向最大匹配结果！" << endl;
	}
	*/
}
/*正向最大匹配算法*/
int ForwardMaximunMatching()
{
	int measure[3] = { 0 };//度量标准 0:非字典词数 1:单字字典词数  2:总词数
	string result;//匹配结果存放
	bool isMatchOrNot = false;//匹配是否成功
	unsigned int startOfWord = 0;//用于匹配的输入句子的起始位置
	unsigned int endOfWord = inputSentence.size() > 4 ? MaxNum - 1 : inputSentence.size() - 1;//用于匹配字典的输入句子的结束位置
	string matchWord;//存放待匹配项
	/*从输入语句中取出待匹配项*/
	for (unsigned int i = startOfWord; i <= endOfWord; i++)
	{
		matchWord += inputSentence[i];
	}
	while (true)
	{
		for (unsigned int i = 0; i < dictionary.size(); i++)
		{
			if (matchWord == dictionary[i])
			{
				/*匹配字典*/
				isMatchOrNot = true;
				measure[2]++;
				if (matchWord.size() == 2)
				{
					measure[1]++;
				}
				result += matchWord;
				F_DataOfSegmentation.push_back(matchWord);
			//	cout << "匹配到：" << matchWord << endl;
				result += "/";
				matchWord = "";
				break;
			}
		}
		if (isMatchOrNot)
		{
			/*匹配成功，更新匹配项首尾指针*/
			if (endOfWord == inputSentence.size()-1)
			{
				break;
			}
			else
			{
				startOfWord = endOfWord + 1;
				endOfWord = ((endOfWord + 4) > inputSentence.size() - 1) ? inputSentence.size() - 1 : endOfWord + 4;
			}
		}
		else
		{
			/*匹配失败，待匹配项长度减一*/
			if (endOfWord == startOfWord)
			{
				result += matchWord;
				F_DataOfSegmentation.push_back(matchWord);
				measure[0]++;
				measure[2]++;
			//	cout << "压入只剩下的汉字：" << matchWord << endl;
				result += "/";
				startOfWord = endOfWord + 1;
				endOfWord = ((endOfWord + 4) > inputSentence.size() - 1) ? inputSentence.size() - 1 : endOfWord + 4;
			}
			else
			{
				endOfWord -= 1;
			}
		}
		/*更新待匹配项*/
		matchWord = "";
		for (unsigned int j = startOfWord; j <= endOfWord; j++)
		{
			matchWord += inputSentence[j];
		}
		isMatchOrNot = false;
		if (matchWord == "")
		{
			break;
		}
	}
	cout << "非字典词数：" << measure[0] << " 单字字典词数：" << measure[1] << " 总词数：" << measure[2] << " 共计：" << measure[0] + measure[1] + measure[2] << endl;
	cout << result << endl;
	cout << endl;
	return measure[0] * 2 + measure[1] + measure[2];
}
/*逆向最大匹配算法*/
int ReverseMaximumMatching()
{
	int measure[3] = { 0 };//度量标准 0:非字典词数 1:单字字典词数  2:总词数
	stack<string> result;//匹配结果存放
	string jieguo;
	result.push("/");
	bool isMatchOrNot = false;//匹配是否成功
	int startOfWord = inputSentence.size() > 5 ? inputSentence.size() - 1 - MaxNum : 0;//用于匹配的输入句子的起始位置
	int endOfWord = inputSentence.size() - 1;//用于匹配字典的输入句子的结束位置
	string matchWord;//存放待匹配项
	/*从输入语句中取出待匹配项*/
	for (auto i = startOfWord; i <= endOfWord; i++)
	{
		matchWord += inputSentence[i];
	}
	while(true)
	{
		for (unsigned int i = 0; i < dictionary.size(); i++)
		{
			if (matchWord == dictionary[i])
			{
				/*匹配字典*/
				isMatchOrNot = true;
				result.push(matchWord);
				measure[2]++;
				if (matchWord.size() == 2)
				{
					measure[1]++;
				}
			//	cout << "匹配到：" << matchWord << endl;
				result.push("/");
				matchWord = "";
				break;
			}
		}
		if (isMatchOrNot)
		{
			/*匹配成功，更新匹配项首尾指针*/
			if (startOfWord == 0)
			{
				break;
			}
			else
			{
				endOfWord = startOfWord - 1;
				startOfWord = startOfWord > 5 ? startOfWord - 5 : 0;
			}
		}
		else
		{
			/*匹配失败，待匹配项长度减一*/
			if (endOfWord - startOfWord == 0)
			{
				result.push(matchWord);
			//	cout << "压入只剩下的汉字：" << matchWord << endl;
				measure[0]++;
				measure[2]++;
				result.push("/");
				endOfWord = startOfWord - 1;
				startOfWord = startOfWord > 5 ? startOfWord - 5 : 0;
			}
			else 
			{
				startOfWord += 1;
			}
		}
		/*更新待匹配项*/
		matchWord = "";
		for (auto j = startOfWord; j <= endOfWord; j++)
		{
			matchWord += inputSentence[j];
		}
		isMatchOrNot = false;
		if (matchWord == "")
		{
			break;
		}
	}
	
	cout << "非字典词数：" << measure[0] << " 单字字典词数：" << measure[1] << " 总词数：" << measure[2] << " 共计：" << measure[0] + measure[1] + measure[2] << endl;
	result.pop();
	while (!result.empty())
	{
		/*将分词结果写入存储分词结果的全局变量*/
		if (result.top() != "/")R_DataOfSegmentation.push_back(result.top());
		jieguo += result.top();
		cout << result.top();
		result.pop();
	}
	cout << endl;
	return measure[0] * 2 + measure[1] + measure[2];
}

/*从文件中读取字典，存入vector变量中*/
void DictFileToData()
{
	string filedata;
	unsigned int pfiledata = 0;
	string tempdata;
	ifstream ofile;
	cout << "read the [dict]" << endl;
	ofile.open("dict.txt");
	while (!ofile.eof())
	{
		filedata += ofile.get();
	}
	ofile.close();
	while (pfiledata < filedata.length())
	{
		if (filedata[pfiledata] == '\n')
		{
			pfiledata++;
			dictionary.push_back(tempdata);
			tempdata = "";
		}
		tempdata += filedata[pfiledata];
		pfiledata++;
	}
}
/*从文件中读取初始状态矩阵、状态转移矩阵、汉字表等数据*/
void ReadDataFromFile()
{
	string filedata;
	unsigned int pfiledata = 0;
	string tempdata;
	ifstream ofile;
	cout << "read the [wordTbl]" << endl;
	ofile.open("wordTbl.txt");
	while (!ofile.eof())
	{
		filedata += ofile.get();
	}
	ofile.close();
	while (pfiledata < filedata.length())
	{
		if (filedata[pfiledata] == '\n')
		{
			pfiledata++;
			wordTbl.push_back(tempdata);
			tempdata = "";
		}
		tempdata += filedata[pfiledata];
		pfiledata++;
	}

	filedata = "";
	pfiledata = 0;
	tempdata = "";
	cout << "read the [dataOfWord[10000][6]]" << endl;
	ofile.open("dataOfWord[10000][6].txt");
	while (!ofile.eof())
	{
		filedata += ofile.get();
	}
	ofile.close();
	int i = 0;
	int j = 0;
	while (pfiledata < filedata.length())
	{
		if (filedata[pfiledata] == ' ')
		{
			dataOfWord[i][j] = stod(tempdata);
			j++;
			pfiledata++;
			tempdata = "";
		}
		else if (filedata[pfiledata] == '\n')
		{
			j = 0;
			i++;
			pfiledata++;
			tempdata = "";
		}
		else 
		{
			tempdata += filedata[pfiledata++];
		}
	}

	filedata = "";
	pfiledata = 0;
	tempdata = "";
	cout << "read the [TransProbMatrix[5][5]]" << endl;
	ofile.open("TransProbMatrix[5][5].txt");
	while (!ofile.eof())
	{
		filedata += ofile.get();
	}
	ofile.close();
	i = 0;
	j = 0;
	while (pfiledata < filedata.length())
	{
		if (filedata[pfiledata] == ' ')
		{
			TransProbMatrix[i][j] = stod(tempdata);
			j++;
			pfiledata++;
			tempdata = "";
		}
		else if (filedata[pfiledata] == '\n')
		{
			j = 0;
			i++;
			pfiledata++;
			tempdata = "";
		}
		else
		{
			tempdata += filedata[pfiledata++];
		}
	}


	filedata = "";
	pfiledata = 0;
	tempdata = "";
	cout << "read the [initStatus[5]]" << endl;
	ofile.open("initStatus[5].txt");
	while (!ofile.eof())
	{
		filedata += ofile.get();
	}
	ofile.close();
	i = 0;
	j = 0;
	while (pfiledata < filedata.length())
	{
		if (filedata[pfiledata] == '\n')
		{
			
		//	ss << tempdata;
		//	ss >> initStatus[i];
			initStatus[i] = stod(tempdata);
			i++;
			pfiledata++;
			tempdata = "";
		}
		else
		{
			tempdata += filedata[pfiledata++];
		}
	}

	filedata = "";
	pfiledata = 0;
	tempdata = "";
	cout << "read the [wordOfPOSTbl]" << endl;
	ofile.open("wordOfPOSTbl.txt");
	while (!ofile.eof())
	{
		filedata += ofile.get();
	}
	ofile.close();
	while (pfiledata < filedata.length())
	{
		if (filedata[pfiledata] == '\n')
		{
			pfiledata++;
			wordOfPOSTbl.push_back(tempdata);
			tempdata = "";
		}
		tempdata += filedata[pfiledata];
		pfiledata++;
	}

	filedata = "";
	pfiledata = 0;
	tempdata = "";
	cout << "read the [dataOfPOSTbl[100000][41]]" << endl;
	ofile.open("dataOfPOSTbl[100000][41].txt");
	while (!ofile.eof())
	{
		filedata += ofile.get();
	}
	ofile.close();
	i = 0;
	j = 0;
	while (pfiledata < filedata.length())
	{
		if (filedata[pfiledata] == ' ')
		{
			dataOfPOSTbl[i][j] = stod(tempdata);
			j++;
			pfiledata++;
			tempdata = "";
		}
		else if (filedata[pfiledata] == '\n')
		{
			j = 0;
			i++;
			pfiledata++;
			tempdata = "";
		}
		else
		{
			tempdata += filedata[pfiledata++];
		}
	}

	filedata = "";
	pfiledata = 0;
	tempdata = "";
	cout << "read the [POSTransProbMatrix[40][40]]" << endl;
	ofile.open("POSTransProbMatrix[40][40].txt");
	while (!ofile.eof())
	{
		filedata += ofile.get();
	}
	ofile.close();
	i = 0;
	j = 0;
	while (pfiledata < filedata.length())
	{
		if (filedata[pfiledata] == ' ')
		{
			POSTransProbMatrix[i][j] = stod(tempdata);
			j++;
			pfiledata++;
			tempdata = "";
		}
		else if (filedata[pfiledata] == '\n')
		{
			j = 0;
			i++;
			pfiledata++;
			tempdata = "";
		}
		else
		{
			tempdata += filedata[pfiledata++];
		}
	}


	filedata = "";
	pfiledata = 0;
	tempdata = "";
	cout << "read the [POSInitStatus[40]]" << endl;
	ofile.open("POSInitStatus[40].txt");
	while (!ofile.eof())
	{
		filedata += ofile.get();
	}
	ofile.close();
	i = 0;
	j = 0;
	while (pfiledata < filedata.length())
	{
		if (filedata[pfiledata] == '\n')
		{

			//	ss << tempdata;
			//	ss >> initStatus[i];
			POSInitStatus[i] = stod(tempdata);
			i++;
			pfiledata++;
			tempdata = "";
		}
		else
		{
			tempdata += filedata[pfiledata++];
		}
	}
}
/*将一次处理得到的数据写入文件当中，以便需要时读取*/
void WriteDataToFile()
{

	ofstream ofile;
	cout << "save the [wordTbl]" << endl;
	ofile.open("wordTbl.txt");
	for (unsigned int i = 0; i < wordTbl.size(); i++)
	{
		ofile << wordTbl[i] << endl;
	}
	ofile.close();

	cout << "save the [dataOfWord[10000][6]]" << endl;
	ofile.open("dataOfWord[10000][6].txt");
	for (unsigned int i = 0; dataOfWord[i][0] != 0; i++)
	{
		for (unsigned int j = 0; j < 6; j++)
		{
			ofile << dataOfWord[i][j] << " ";
		}
		ofile << endl;
	}
	ofile.close();

	cout << "save the [TransProbMatrix[5][5]]" << endl;
	ofile.open("TransProbMatrix[5][5].txt");
	for (unsigned int i = 0; i<5; i++)
	{
		for (unsigned int j = 0; j < 5; j++)
		{
			ofile << TransProbMatrix[i][j] << " ";
		}
		ofile << endl;
	}
	ofile.close();

	cout << "save the [initStatus[5]]" << endl;
	ofile.open("initStatus[5].txt");
	for (unsigned int i = 0; i < 5; i++)
	{
		ofile << initStatus[i] << endl;
	}
	ofile.close();

	cout << "save the [wordOfPOSTbl]" << endl;
	ofile.open("wordOfPOSTbl.txt");
	for (unsigned int i = 0; i < wordOfPOSTbl.size(); i++)
	{
		ofile << wordOfPOSTbl[i] << endl;
	}
	ofile.close();

	cout << "save the [dataOfPOSTbl[100000][41]]" << endl;
	ofile.open("dataOfPOSTbl[100000][41].txt");
	for (unsigned int i = 0; dataOfPOSTbl[i][0] != 0; i++)
	{
		for (unsigned int j = 0; j < 41; j++)
		{
			ofile << dataOfPOSTbl[i][j] << " ";
		}
		ofile << endl;
	}
	ofile.close();

	cout << "save the [POSTransProbMatrix[40][40]]" << endl;
	ofile.open("POSTransProbMatrix[40][40].txt");
	for (unsigned int i = 0; i < 40; i++)
	{
		for (unsigned int j = 0; j < 40; j++)
		{
			ofile << POSTransProbMatrix[i][j] << " ";
		}
		ofile << endl;
	}
	ofile.close();

	cout << "save the [POSInitStatus[40]]" << endl;
	ofile.open("POSInitStatus[40].txt");
	for (unsigned int i = 0; i < 40; i++)
	{
		ofile << POSInitStatus[i] << endl;
	}
	ofile.close();
}

/*找到汉字i为状态j的最大概率*/
double FindMax(double *prob)
{
	double temp = prob[0];
	for (unsigned int i = 1; i < 5; i++)
	{
		if (prob[i] > temp)temp = prob[i];
	}
	return temp;
}
double posFindMax(double *prob)
{
	double temp = prob[0];
	for (unsigned int i = 1; i < 40; i++)
	{
		if (prob[i] > temp)temp = prob[i];
	}
	return temp;
}

/*最大概率节点*/
int FindMaxNum(double *prob)
{
	double temp = prob[0];
	int cnt = 0;
	for (unsigned int i = 1; i < 5; i++)
	{
		if (prob[i] > temp)
		{
			temp = prob[i];
			cnt = i;
		}
	}
	return cnt;
}
int posFindMaxNum(double *prob)
{
	double temp = prob[0];
	int cnt = 0;
	for (unsigned int i = 1; i < 40; i++)
	{
		if (prob[i] > temp)
		{
			temp = prob[i];
			cnt = i;
		}
	}
	return cnt;
}

void GetPosProbOderOfStatus(double**posemitprobmatrix)
{
	stack<int> s;
	double maxProbOfDestination;
	int nodeOfDes;
	int **previousMaxProb = new int*[DataOfSegmentation.size()];
	double **posProbOderOfStatus = new double *[DataOfSegmentation.size()];
	for (unsigned int i = 0; i < DataOfSegmentation.size(); i++)
	{
		previousMaxProb[i] = new int[40];
	}
	for (unsigned int i = 0; i < DataOfSegmentation.size(); i++)
	{
		posProbOderOfStatus[i] = new double[40];
	}
	for (unsigned int i = 0; i < DataOfSegmentation.size(); i++)
	{
		//cout << DataOfSegmentation[i] << ":" << endl;
		if (i == 0)
		{
			for (unsigned int j = 0; j < 40; j++)
			{
				//初始状态，为初始状态概率与第一个汉字对应状态发射概率概率之和
				posProbOderOfStatus[i][j] = POSInitStatus[j] + posemitprobmatrix[i][j];
				previousMaxProb[i][j] = 0;
				//cout << posProbOderOfStatus[i][j] << " ";
			}
		//	cout << endl;
		}
		else
		{
			for (unsigned int j = 0; j < 40; j++)
			{
				double temp[40];
				double weight = 0;
				for (unsigned int k = 0; k < 40; k++)
				{
				//	//求上一汉字观测为状态k，目前汉字观测为状态j的发射概率，以及两个状态转移概率之和
					temp[k] = posProbOderOfStatus[i - 1][k] + posemitprobmatrix[i][j] +  POSTransProbMatrix[k][j] + weight;
				//	cout << "上一词汇[" << DataOfSegmentation[i - 1] << "]词性为[" << POSTbl[k] << "] prob:[" << posProbOderOfStatus[i - 1][k] << "]" << endl;
				//	cout << "目前词汇[" << DataOfSegmentation[i] << "]观测为[" << POSTbl[j] << "] prob:[" << posemitprobmatrix[i][j] << "]" << endl;
				//	cout << "以及两个词性转移概率 Prob:[" <<  POSTransProbMatrix[k][j] << "]以及权重：[" << weight << "]之和：" << temp[k] << endl;
					//weight = 0;
				}
				posProbOderOfStatus[i][j] = posFindMax(temp);//使得第i个汉字状态为j的最大概率，即第i个汉字状态为SBMD中的某个状态，且该汉字为该状态的概率最大
				previousMaxProb[i][j] = posFindMaxNum(temp);//使得第i个汉字状态为j的概率最大的（状态）结点
			//	cout << "使得词汇[" << DataOfSegmentation[i] << "]为词性[" << POSTbl[j] << "]的概率最大的词汇[" << DataOfSegmentation[i - 1] << "]的词性为： [" << POSTbl[previousMaxProb[i][j]] << "]" << endl << endl;
			}
		}
	}
	maxProbOfDestination = posFindMax(posProbOderOfStatus[DataOfSegmentation.size() - 1]);
	nodeOfDes =posFindMaxNum(posProbOderOfStatus[DataOfSegmentation.size() - 1]);
//	cout << "词汇[" << DataOfSegmentation[DataOfSegmentation.size() - 1] << "]" << "为词性[" << POSTbl[nodeOfDes] << "]的概率最大" << endl;
	s.push(nodeOfDes);
	for (unsigned int i = 0; i < DataOfSegmentation.size() - 1; i++)
	{
		s.push(previousMaxProb[DataOfSegmentation.size() - i - 1][nodeOfDes]);
		nodeOfDes = previousMaxProb[DataOfSegmentation.size() - i - 1][nodeOfDes];
	//	cout << "词汇[" << DataOfSegmentation[DataOfSegmentation.size() - 2 - i] << "]" << "为词性[" << POSTbl[nodeOfDes] << "]的概率最大" << endl;
	}
	for (unsigned int i = 0; i < DataOfSegmentation.size(); i++)
	{
		int Node;
		Node = s.top();
		cout << DataOfSegmentation[i];
		cout << "/" << POSTbl[Node];
		s.pop();
	}
	cout << endl;
	}

/*获得状态序列，维特比算法*/
void GetProbOderOfStatus(double**emitprobmatrix)
{
	stack<int> s;
	double maxProbOfDestination;
	int nodeOfDes;
	/*
	一个长度为输入句子的汉字个数的二维数组，第一维大小为汉字个数，第二维大小为每个汉字的隐藏状态个数，
	记录每个汉字取不同的隐藏状态的最大概率值。由于前一汉字取某一状态的概率会影响到下一状态，所以需要记录。
	并且最后需要从最后一个汉字的取某一状态时的概率最大，以此从后往前找到整句话的状态序列。
	*/
	int **previousMaxProb = new int*[inputSentence.size()];
	/*
	一个长度为输入句子的汉字个数的二维数组，第一维大小为汉字个数，第二维大小为每个汉字的隐藏状态个数，
	记录每个汉字取某一状态时，他的前一个汉字所选状态。（所以状态序列是从最后向前推测的）。
	比如，依据上述的previousMaxProb的最后一个汉字选取各个状态的概率，
	选择概率最大的状态，则在本变量当中可以往前推测整个句子的状态序列。
	*/
	double **ProbOderOfStatus = new double *[inputSentence.size()];
	for (unsigned int i = 0; i < inputSentence.size(); i++)
	{
		previousMaxProb[i] = new int[4];
	}
	for (unsigned int i = 0; i < inputSentence.size(); i++)
	{
		ProbOderOfStatus[i] = new double[4];
	}
	double times = 1;
	//cout << "times:";
	//cin >> times;
	for (unsigned int i = 0; i < inputSentence.size(); i++)
	{
		//cout << inputSentence[i] << ":" << endl;
		if (i == 0)
		{
			for (unsigned int j = 0; j < 5; j++)
			{
				//初始状态，为初始状态概率与第一个汉字对应状态发射概率概率之和
				ProbOderOfStatus[i][j] = initStatus[j] + emitprobmatrix[i][j];//发射概率：输入句子中的某个汉字为某一状态的概率。
				previousMaxProb[i][j] = 0;
			//	cout << ProbOderOfStatus[i][j] << " ";
			}
			cout << endl;
		}
		else
		{
			for (unsigned int j = 0; j < 5; j++)
			{
				double temp[5];
				double weight = 0;
				for (unsigned int k = 0; k < 5; k++)
				{
					//求上一汉字观测为状态k，目前汉字观测为状态j的发射概率，以及两个状态转移概率之和
					temp[k] = ProbOderOfStatus[i - 1][k] + emitprobmatrix[i][j] + times * TransProbMatrix[k][j] + weight;
					//cout << "上一汉字[" << inputSentence[i - 1] << "]状态为[" << status[k] << "] prob:[" << ProbOderOfStatus[i - 1][k] << "]" << endl;
					//cout << "目前汉字[" << inputSentence[i] << "]观测为[" << status[j] << "] prob:[" << emitprobmatrix[i][j] << "]" << endl;
					//cout << "以及两个状态转移概率 Prob:[" << times * TransProbMatrix[k][j] << "]以及权重：[" << weight << "]之和：" << temp[k] << endl;
					//weight = 0;
				}
				ProbOderOfStatus[i][j] = FindMax(temp);/*上一个汉字为某一个状态，使得第i个（当前）汉字状态为j的概率最大，即第i个汉字状态为SBME中的某个状态，且该汉字为该状态的概率最大*/
				previousMaxProb[i][j] = FindMaxNum(temp);//确定上一个汉字为某一个状态，使得第i个汉字状态为j的概率最大的（状态）结点
				//cout << "使得汉字[" << inputSentence[i] << "]为状态[" << status[j] << "]的概率最大的汉字[" << inputSentence[i - 1] << "]的状态为： [" << status[previousMaxProb[i][j]] << "]" << endl << endl;
			}
		}
	}
	maxProbOfDestination = FindMax(ProbOderOfStatus[inputSentence.size() - 1]);
	nodeOfDes = FindMaxNum(ProbOderOfStatus[inputSentence.size() - 1]);
	//cout << "汉字[" << inputSentence[inputSentence.size() - 1] << "]" << "为状态[" << status[nodeOfDes] << "]的概率最大" << endl;
	s.push(nodeOfDes);
	for (unsigned int i = 0; i < inputSentence.size() - 1; i++)
	{
		s.push(previousMaxProb[inputSentence.size() - i - 1][nodeOfDes]);
		nodeOfDes = previousMaxProb[inputSentence.size() - i - 1][nodeOfDes];
		//cout << "汉字[" << inputSentence[inputSentence.size() - 2 - i] << "]" << "为状态[" << status[nodeOfDes] << "]的概率最大" << endl;
	}
	string subword;
	for (unsigned int i = 0; i < inputSentence.size(); i++)
	{
		int Node;
		Node = s.top();
		subword += inputSentence[i];
		cout << inputSentence[i];
		switch (Node)
		{
		case 0:
		{
			HMM_DataOfSegmentation.push_back(subword);
			subword = "";
			cout << "/"; break;
		}
		case 1:cout << ""; break;
		case 2:cout << ""; break;
		case 3:cout << ""; break;
		case 4:
		{
			HMM_DataOfSegmentation.push_back(subword);
			subword = "";
			cout << "/"; break;
		}
		default:
			break;
		}
		s.pop();
	}
	cout << endl;
}

/*HMM分析效果判断*/
int EffectOfHMMSeg()
{
	int measure[3] = { 0 };//度量标准 0:非字典词数 1:单字字典词数  2:总词数
	for (unsigned int i = 0; i < HMM_DataOfSegmentation.size(); i++)
	{
		int mark = false;
		for (unsigned int j = 0; j < dictionary.size(); j++)
		{
			if (HMM_DataOfSegmentation[i] == dictionary[j])
			{
				mark = true;
				measure[2]++;
				if (HMM_DataOfSegmentation[i].size() == 2)
				{
					measure[1]++;
				}
				break;
			}
		}
		if (!mark)
		{
			measure[0]++;
			measure[2]++;
		}
	}
	cout << "非字典词数：" << measure[0] << " 单字字典词数：" << measure[1] << " 总词数：" << measure[2] << " 共计：" << measure[0] + measure[1] + measure[2] << endl;
	return measure[0] * 2 + measure[1] + measure[2];
}

/*打印发射概率矩阵*/
void ShowEmitProbMatrix(double**matrix)
{
	cout << "对应汉字发射概率：" << endl;
	for (unsigned int i = 0; i < inputSentence.size(); i++)
	{
		cout << inputSentence[i] << ":";
		for (unsigned int j = 0; j < 5; j++)
		{
			cout << matrix[i][j] << " ";
		}
		cout << endl;
	}
}

/*找到发射概率矩阵*/
double** FindEmitProbMatrix()
{
	double **EmitProbMatrix = new double *[inputSentence.size()];
	for (unsigned int i = 0; i < inputSentence.size(); i++)
	{
		EmitProbMatrix[i] = new double[5];
	}
	for (unsigned int i = 0; i < inputSentence.size(); i++)
	{
		bool mark = false;//标记是否在汉字表中有该汉字，以便决定是否进行平滑处理
		for (unsigned int j = 0; j < wordTbl.size(); j++)
		{
			if (inputSentence[i] == wordTbl[j])
			{
				mark = true;
				for (unsigned int k = 0; k < 5; k++)
				{
					EmitProbMatrix[i][k] = dataOfWord[j][k + 1];
				}
			}
		}
		if (!mark)
		{
			for (unsigned int k = 0; k < 5; k++)
			{
				EmitProbMatrix[i][k] = log(0.25);
			}
		}
	}
	return EmitProbMatrix;
}

/*处理需要进行分词的句子*/
void SentenceToData()
{
	string sentence;
	string subword;
	//int pSentence = 0;
	int cnt = 0;
	cout << "please input the sentence that you want to process:";
	cin >> sentence;
	while (sentence != "")
	{
		if ((sentence[0] >= '0'&&sentence[0] <= '9') || (sentence[0] >= 'a'&&sentence[0] <= 'z') || (sentence[0] >= 'A'&&sentence[0] <= 'Z'))
		{
			if (cnt >= 1)
			{
				inputSentence.push_back(subword);
			}
			else
			{
				subword += sentence[0];
				inputSentence.push_back(subword);
			}
			sentence.erase(0, 1);
			cnt = 0;
			subword = "";
		}
		else
		{
			subword += sentence[0];
			sentence.erase(0, 1);
			cnt++;
			if (cnt >= 2)
			{
				inputSentence.push_back(subword);
				subword = "";
				cnt = 0;
			}
		}
	}
}

/*获取初始向量*/
void GetInitStatus(int previous, int mark)
{
	if (previous == -1)
	{
		initStatus[mark]++;
	}
}

/*将矩阵从频数转为自然对数*/
void FloatToIn()
{
	double sum = 0;
	/*
	for (unsigned int i = 0; i < 4; i++)
	{
		for (unsigned int j = 0; j < 4; j++)
		{
			sum += TransProbMatrix[i][j];
		}
	}
	*/
	for (unsigned int i = 0; i < 5; i++)
	{
		for (unsigned int m = 0; m < 5; m++)
		{
			sum += TransProbMatrix[i][m];
		}
		for (unsigned int j = 0; j < 5; j++)
		{
			if (TransProbMatrix[i][j] != 0)
			{
				TransProbMatrix[i][j] = log(TransProbMatrix[i][j] / sum);
			}
			else
			{
				TransProbMatrix[i][j] = -3.14e+100;
			}
		}
		sum = 0;
	}
	sum = 0;
	for (unsigned int i = 0; i < 5; i++)
	{
		sum += initStatus[i];
	}
	for (unsigned int i = 0; i < 5; i++)
	{
		if (initStatus[i] != 0)
		{
			initStatus[i] = log(initStatus[i] / sum);
		}
		else
		{
			initStatus[i] = -3.14e+100;
		}
	}
	sum = 0;
	int cnt = 0;
	while (dataOfWord[cnt][0] != 0)
	{
		/*+1平滑处理*/
		dataOfWord[cnt][0] += 5;
		for (unsigned int i = 0; i < 5; i++)
		{
			dataOfWord[cnt][i + 1] = log((dataOfWord[cnt][i + 1] + 1) / dataOfWord[cnt][0]);
		}
		/*
		for (unsigned int i = 0; i < 4; i++)
		{
			if (dataOfWord[cnt][i + 1] != 0)
			{
				dataOfWord[cnt][i + 1] = log(dataOfWord[cnt][i + 1]  / dataOfWord[cnt][0]);
			}
			else
			{
				dataOfWord[cnt][i + 1] = -3.14e+100;
			}
		}
		*/
		cnt++;
	}

	/*记录各种词性的出现频率*/
	for (auto n = 0; n < 41; n++)
	{
		tagProb[n] = (n == 0) ? 40 : 1;
		for (auto i = 0; dataOfPOSTbl[i][0] != 0; i++)
		{
			tagProb[n] += dataOfPOSTbl[i][n];
		}
	}
	for (auto i = 1; i < 41; i++)
	{
		tagProb[i] = log(tagProb[i] / tagProb[0]);
	}

	sum = 0;
	for (unsigned int i = 0; i < 40; i++)
	{
		for (unsigned int m = 0; m < 40; m++)
		{
			sum += POSTransProbMatrix[i][m];
		}
		for (unsigned int j = 0; j < 40; j++)
		{
			if (POSTransProbMatrix[i][j] != 0)
			{
				POSTransProbMatrix[i][j] = log(POSTransProbMatrix[i][j] / sum);
			}
			else
			{
				POSTransProbMatrix[i][j] = -3.14e+100;
			}
		}
		sum = 0;
	}
	sum = 0;
	for (unsigned int i = 0; i < 40; i++)
	{
		sum += POSInitStatus[i];
	}
	for (unsigned int i = 0; i < 40; i++)
	{
		if (POSInitStatus[i] != 0)
		{
			POSInitStatus[i] = log(POSInitStatus[i] / sum);
		}
		else
		{
			POSInitStatus[i] = -3.14e+100;
		}
	}
	sum = 0;
	cnt = 0;
	while (dataOfPOSTbl[cnt][0] != 0)
	{
		/*+1平滑处理*/

		/*
		dataOfPOSTbl[cnt][0] += 40;
		for (unsigned int i = 0; i < 40; i++)
		{
			dataOfPOSTbl[cnt][i + 1] = log((dataOfPOSTbl[cnt][i + 1] + 1) / dataOfPOSTbl[cnt][0]);
		}
		*/

		for (unsigned int i = 0; i < 40; i++)
		{
			if (dataOfPOSTbl[cnt][i + 1] != 0)
			{
				dataOfPOSTbl[cnt][i + 1] = log(dataOfPOSTbl[cnt][i + 1] / dataOfPOSTbl[cnt][0]);
			}
			else
			{
				dataOfPOSTbl[cnt][i + 1] = -3.14e+100;
			}
		}
		cnt++;
	}
}

/*更新状态转移矩阵*/
void UpDateMatrix(int previous, int mark)
{
	if (previous == -1)return;
	else
	{
		TransProbMatrix[previous][mark]++;
	}
}

/*根据subword返回其在wordTbl中的位置*/
int PositionOfSubword(string subword)
{
	for (unsigned int i = 0; i < wordTbl.size(); i++)
	{
		if (subword == wordTbl[i])
		{
			return i;
		}
	}
	return -1;
}

/*依据标记mark将汉字的sbmd属性更新*/
void WritePosition(int mark)
{
	switch (mark)
	{
	case 0:
	{
		dataOfWord[numOfWord][1]++; break;
	}
	case 1:
	{
		dataOfWord[numOfWord][2]++; break;
	}
	case 2:
	{
		dataOfWord[numOfWord][3]++; break;
	}
	case 3:
	{
		dataOfWord[numOfWord][4]++; break;
	}
	case 4:
	{
		dataOfWord[numOfWord][5]++; break;
	}
	default:
		break;
	}
}

/*根据mark，position更新汉字的SBMD属性*/
void WritePosition(int mark, int position)
{
	switch (mark)
	{
	case 0:
	{
		dataOfWord[position][1]++; break;
	}
	case 1:
	{
		dataOfWord[position][2]++; break;
	}
	case 2:
	{
		dataOfWord[position][3]++; break;
	}
	case 3:
	{
		dataOfWord[position][4]++; break;
	}
	case 4:
	{
		dataOfWord[position][5]++; break;
	}
	default:
		break;
	}
}

/*判断本次读取的汉字是否重复，重复则返回true，否则返回false*/
bool IsRepeatOrNot(string word)
{
	for (unsigned int i = 0; i < wordTbl.size(); i++)
	{
		if (word == wordTbl[i])
		{
			dataOfWord[i][0]++;
			return true;
		}
	}
	return false;
}
/*词汇在wordOfPosTbl中的位置，找不到则返回-1*/
int FindWordInPosTbl(string word)
{
	for (unsigned int i = 0; i < wordOfPOSTbl.size(); i++)
	{
		if (word == wordOfPOSTbl[i])
		{
			return i;
		}
	}
	return -1;
}

/*处理语料库，得到模型三要素：初始向量，状态转移概率，发射概率*/
void DealWithCorpus()
{
	string filename = "D:\\199806.txt";
	string corpus = { "" };
	string subword = { "" };
	ifstream openfile;
	cout << "Please input the path of corpus: ";
	cin >> filename;
	openfile.open(filename);
	while (!openfile.is_open())
	{
		cout << "Can't find the file base the path you gave! Please retry: ";
		cin >> filename;
		openfile.open(filename);
	}
	cout<< "sourcefile open finished!" << endl;
	while (!openfile.eof())
	{
		corpus += openfile.get();
	}
	openfile.close();
	cout << "sourcefile loading finished!" << endl;
	cout << "start to deal with corpus..." << endl;
	int cnt = 0;
	int mark = 0;//0：single 1：begin 2：beginToMiddle 3：middle  4:end
	int wordcnt = 0;//记录在遇到 / 前被扫描的汉字个数，每当扫描到 / 后清零
	int previousStatus = -1;//标记前一状态，-1为句首，0、1、2、3、4、：S、B、BM、M、E、
	unsigned int pcorpus = 0;//语料库指针
	string tcorpus;//语料库
	while (corpus[pcorpus] != EOF)
	{
		if (corpus[pcorpus] == '1'&&corpus[pcorpus + 1] == '9')
		{
			pcorpus += 23;
		}
		else
		{
			tcorpus += corpus[pcorpus++];
		}
	}

	corpus = tcorpus;
	pcorpus = 0;
	while (pcorpus <= corpus.length())
	{
		if (corpus[pcorpus] != '/')
		{
			if (corpus[pcorpus] == '[')
			{
				//corpus.erase(0, 1);
				pcorpus++;
			}
			if (corpus[pcorpus] >= '0'&&corpus[pcorpus] <= '9')
			{
				if (cnt >= 1)
				{
					if (!IsRepeatOrNot(subword))//不重复时
					{
						wordTbl.push_back(subword);
						dataOfWord[numOfWord][0]++;
						if (corpus[pcorpus] == '/')
						{
							if (wordcnt == 0)mark = 0;
							else
							{
								mark = 4;
							}
						}
						else
						{
							if (wordcnt == 0)
							{
								mark = 1;
							}
							else if(wordcnt==1)
							{
								mark = 2;
							}
							else
							{
								mark = 3;
							}
						}
					//	cout << "(previous:" << previousStatus << ",mark:" << mark << ")";
						UpDateMatrix(previousStatus, mark);
						GetInitStatus(previousStatus, mark);
						previousStatus = mark;
						WritePosition(mark);
						wordcnt++;
						numOfWord++;
					//	cout << subword;
					}
					else
					{
						//重复时，汉字SBME数据需要另作更新
						if (corpus[pcorpus] == '/')
						{
							if (wordcnt == 0)mark = 0;
							else
							{
								mark = 4;
							}
						}
						else
						{
							if (wordcnt == 0)
							{
								mark = 1;
							}
							else if (wordcnt == 1)
							{
								mark = 2;
							}
							else
							{
								mark = 3;
							}
						}
					//	cout << "(previous:" << previousStatus << ",mark:" << mark << ")";
					//	cout << subword;
						UpDateMatrix(previousStatus, mark);
						GetInitStatus(previousStatus, mark);
						previousStatus = mark;
						WritePosition(mark, PositionOfSubword(subword));
						wordcnt++;
					}
					subword = "";
				}
				subword += corpus[pcorpus];
				pcorpus++;
				if (!IsRepeatOrNot(subword))//不重复时
				{
					wordTbl.push_back(subword);
					dataOfWord[numOfWord][0]++;
					if (corpus[pcorpus] == '/')
					{
						if (wordcnt == 0)mark = 0;
						else
						{
							mark = 4;
						}
					}
					else
					{
						if (wordcnt == 0)
						{
							mark = 1;
						}
						else if (wordcnt == 1)
						{
							mark = 2;
						}
						else
						{
							mark = 3;
						}
					}
				//	cout << "(previous:" << previousStatus << ",mark:" << mark << ")";
					UpDateMatrix(previousStatus, mark);
					GetInitStatus(previousStatus, mark);
					previousStatus = mark;
					WritePosition(mark);
					wordcnt++;
					numOfWord++;
				//	cout << subword;
				}
				else
				{
					//重复时，汉字SBME数据需要另作更新
					if (corpus[pcorpus] == '/')
					{
						if (wordcnt == 0)mark = 0;
						else
						{
							mark = 4;
						}
					}
					else
					{
						if (wordcnt == 0)
						{
							mark = 1;
						}
						else if (wordcnt == 1)
						{
							mark = 2;
						}
						else
						{
							mark = 3;
						}
					}
				//	cout << "(previous:" << previousStatus << ",mark:" << mark << ")";
				//	cout << subword;
					UpDateMatrix(previousStatus, mark);
					GetInitStatus(previousStatus, mark);
					previousStatus = mark;
					WritePosition(mark, PositionOfSubword(subword));
					wordcnt++;
				}
				subword = "";
				cnt = 0;
			}
			else
			{
				subword += corpus[pcorpus];
				pcorpus++;
				cnt++;
				if (cnt >= 2)
				{
					if (!IsRepeatOrNot(subword))//不重复时
					{
						wordTbl.push_back(subword);
						dataOfWord[numOfWord][0]++;
						if (corpus[pcorpus] == '/')
						{
							if (wordcnt == 0)mark = 0;
							else
							{
								mark = 4;
							}
						}
						else
						{
							if (wordcnt == 0)
							{
								mark = 1;
							}
							else if (wordcnt == 1)
							{
								mark = 2;
							}
							else
							{
								mark = 3;
							}
						}
					//	cout << "(previous:" << previousStatus << ",mark:" << mark << ")";
						UpDateMatrix(previousStatus, mark);
						GetInitStatus(previousStatus, mark);
						previousStatus = mark;
						WritePosition(mark);
						wordcnt++;
						numOfWord++;
					//	cout << subword;
					}
					else
					{
						//重复时，汉字SBME数据需要另作更新
						if (corpus[pcorpus] == '/')
						{
							if (wordcnt == 0)mark = 0;
							else
							{
								mark = 4;
							}
						}
						else
						{
							if (wordcnt == 0)
							{
								mark = 1;
							}
							else if (wordcnt == 1)
							{
								mark = 2;
							}
							else
							{
								mark = 3;
							}
						}
					//	cout << "(previous:" << previousStatus << ",mark:" << mark << ")";
					//	cout << subword;
						UpDateMatrix(previousStatus, mark);
						GetInitStatus(previousStatus, mark);
						previousStatus = mark;
						WritePosition(mark, PositionOfSubword(subword));
						wordcnt++;
					}
					subword = "";
					cnt = 0;
				}
			}

		}
		else if (corpus[pcorpus] == '/')
		{
			pcorpus++;
			wordcnt = 0;
			while (true)
			{
				if (corpus[pcorpus] != ' ' && corpus[pcorpus] != '\n')
				{
					pcorpus++;
				}
				else if (corpus[pcorpus] == ' ' || corpus[pcorpus] == '\n')
				{
					while (corpus[pcorpus] == ' ' || corpus[pcorpus] == '\n')
					{
						if (corpus[pcorpus] == '\n')
						{
							//每读到语料库中下一句子，previousStatus置-1，表示无前一状态
							previousStatus = -1;
						}
						pcorpus++;
					}
					break;
				}
			}
		}
	}
	cout << "work finished!" << endl;
}

/*处理语料库得到词性标注信息*/
void DealWithCorpusForPOS()
{
	string filename = "D:\\199806.txt";
	string corpus = { "" };//语料库字符串
	string subword = { "" };//中间字符串（词汇）
	ifstream openfile;
	cout << "Please input the path of corpus: ";
	cin >> filename;
	openfile.open(filename);
	while (!openfile.is_open())
	{
		cout << "Can't find the file base the path you gave! Please retry: ";
		cin >> filename;
		openfile.open(filename);
	}
	cout << "sourcefile open finished!" << endl;
	while (!openfile.eof())
	{
		corpus += openfile.get();
	}
	openfile.close();
	cout << "sourcefile loading finished!" << endl;
	cout << "start to deal with corpus..." << endl;
	int previousPos = -1;//前一时刻词性
	int currentPos;//现在时刻词性
	int cnt = 0;
	int wordcnt = 0;//记录在遇到 / 前被扫描的汉字个数，每当扫描到 / 后清零
	unsigned int pcorpus = 0;//语料库指针
	string tcorpus;//去除语料库开头日期信息的语料库字符串
	while (corpus[pcorpus] != EOF)
	{
		if (corpus[pcorpus] == '1'&&corpus[pcorpus + 1] == '9')
		{
			pcorpus += 23;
		}
		else
		{
			tcorpus += corpus[pcorpus++];
		}
	}

	corpus = tcorpus;
	pcorpus = 0;

	while (pcorpus < corpus.length())
	{
		if (corpus[pcorpus] != '['&&corpus[pcorpus] != ']'&&corpus[pcorpus] != '/'&&corpus[pcorpus]!='\n'&&corpus[pcorpus]!=' ')
		{
			subword += corpus[pcorpus];
			pcorpus++;
		}
		else if (corpus[pcorpus] == '['||corpus[pcorpus]=='\n'||corpus[pcorpus]==' ')
		{
			if (corpus[pcorpus] == '\n')
			{
				previousPos = -1;
			}
			pcorpus++;
		}
		else if (corpus[pcorpus] == ']')
		{
			pcorpus += 5;
		}
		else if (corpus[pcorpus] == '/')
		{
			int position = FindWordInPosTbl(subword);//目前读取的词汇在词汇表中的位置，不在词汇表则为-1
			if (position == -1)
			{
				/*读取的词汇为新词汇*/
				wordOfPOSTbl.push_back(subword);
				subword = "";
				/*此时已读取完毕一个词汇，需读取此词汇的词性*/
				pcorpus++;//指针指向 "/" 之后的符号
				while (corpus[pcorpus] != ' '&&corpus[pcorpus] != ']')
				{
					subword += corpus[pcorpus];
					pcorpus++;
				}
				currentPos = FindPosOfWord(subword);
				if (currentPos == 39)cout << subword << " ";
				/*更新该词汇的词性数据*/
				dataOfPOSTbl[wordOfPOSTbl.size() - 1][0]++;
				dataOfPOSTbl[wordOfPOSTbl.size() - 1][currentPos + 1]++;
				/*更新初始词性向量矩阵*/
				UpdateInitPosStatus(previousPos, currentPos);
				/*更新词性状态转移矩阵*/
				UpdatePosTransProbMatrix(previousPos, currentPos);
				previousPos = currentPos;
				subword = "";
			}
			else
			{
				subword = "";
				/*此时已读取完毕一个词汇，需读取此词汇的词性*/
				pcorpus++;//指针指向 "/" 之后的符号
				while (corpus[pcorpus] != ' '&&corpus[pcorpus] != ']')
				{
					subword += corpus[pcorpus];
					pcorpus++;
				}
				currentPos = FindPosOfWord(subword);
				if (currentPos == 39)cout << subword << " ";
				/*更新该词汇的词性数据*/
				dataOfPOSTbl[position][0]++;
				dataOfPOSTbl[position][currentPos + 1]++;
				/*更新初始词性向量矩阵*/
				UpdateInitPosStatus(previousPos, currentPos);
				/*更新词性状态转移矩阵*/
				UpdatePosTransProbMatrix(previousPos, currentPos);
				previousPos = currentPos;
				subword = "";
			}
		}
	}
}

/*在词性表中找到待确定词性，找不到返回-1*/
int FindPosOfWord(string pos)
{
	for (unsigned int i = 0; i < sizeof(POSTbl)/sizeof(POSTbl[0]); i++)
	{
		if (pos == POSTbl[i])
		{
			return i;
		}
	}
	return 39;
}

/*更新词性初始状态矩阵*/
void UpdateInitPosStatus(int previous, int current)
{
	if (previous == -1)
	{
		POSInitStatus[current]++;
	}
}
/*更新词性状态转移矩阵*/
void UpdatePosTransProbMatrix(int previous, int current)
{
	if (previous == -1)return;
	else
	{
		POSTransProbMatrix[previous][current]++;
	}
}

/*找到词汇的发射概率矩阵*/
double**FindPosEmitProbMatrix()
{
	double **PosEmitProbMatrix = new double *[DataOfSegmentation.size()];
	for (unsigned int i = 0; i < DataOfSegmentation.size(); i++)
	{
		PosEmitProbMatrix[i] = new double[40];
	}
	for (unsigned int i = 0; i < DataOfSegmentation.size(); i++)
	{
		bool mark = false;//标记是否在汉字表中有该汉字，以便决定是否进行平滑处理
		for (unsigned int j = 0; j < wordOfPOSTbl.size(); j++)
		{
			if (DataOfSegmentation[i] == wordOfPOSTbl[j])
			{
				mark = true;
				for (unsigned int k = 0; k < 40; k++)
				{
					PosEmitProbMatrix[i][k] = dataOfPOSTbl[j][k + 1];
				}
			}
		}
		if (!mark)
		{
			cout << "词性标注未匹配到：" << DataOfSegmentation[i] << endl;
			for (unsigned int k = 0; k < 40; k++)
			{
				PosEmitProbMatrix[i][k] = tagProb[k + 1];
				/*未识别词为人名、地名、时间名、数词的概率较大，故单独增加概率*/
				if (POSTbl[k] == "nr"||POSTbl[k]=="ns"||POSTbl[k]=="t"||POSTbl[k]=="m")PosEmitProbMatrix[i][k] += 1.5;
				if (POSTbl[k] == "w")PosEmitProbMatrix[i][k] -= 10;
				cout << POSTbl[k] <<":"<< PosEmitProbMatrix[i][k] << " ";
			}
			cout << endl;
		}
	}
	return PosEmitProbMatrix;
}

void ShowPosEmitProbMatrix(double **matrix)
{
	cout << "对应词汇发射概率：" << endl;
	for (unsigned int i = 0; i < DataOfSegmentation.size(); i++)
	{
		cout <<DataOfSegmentation[i] << ":";
		for (unsigned int j = 0; j < 40; j++)
		{
			cout << matrix[i][j] << " ";
		}
		cout << endl;
	}
}