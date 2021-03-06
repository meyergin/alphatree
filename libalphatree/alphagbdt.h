//
// Created by godpgf on 18-3-29.
//

#ifndef ALPHATREE_ALPHAGBDT_H
#define ALPHATREE_ALPHAGBDT_H

#include "base/iterator.h"
#include "base/vector.h"
#include "alphaforest.h"
#include "cart/gbdt.h"

class AlphaGBDT{
public:
    static void initialize(AlphaForest* af, const char* alphatreeList, int alphatreeNum, float gamma, float lambda, int threadNum, const char* lossFunName = "binary:logistic"){
        alphaGBDT_ = new AlphaGBDT(af, alphatreeList, alphatreeNum, gamma, lambda, threadNum, lossFunName);
    }

    static void release(){
        if(alphaGBDT_ != nullptr)
            delete alphaGBDT_;
        alphaGBDT_ = nullptr;
    }

    static AlphaGBDT* getAlphaGBDT(){ return alphaGBDT_;}

    //把数据读出去，主要为了测试
    int testReadData(size_t daybefore, size_t sampleSize, const char* target, const char* signName, float* pFeature, float* pTarget, int cacheSize = 4096){
        size_t signNum = af_->getAlphaDataBase()->getSignNum(daybefore, sampleSize, signName);
        //填写特征
        Vector<IBaseIterator<float>*> features( featureList_->getSize());
        for(size_t i = 0; i < features.getSize(); ++i){
            //cout<<(*alphaNameList_)[i].c_str()<<" "<<daybefore<<" "<<sampleSize<<endl;
            features[i] = new AlphaSignIterator(af_, (*alphaNameList_)[i].c_str(), signName, (*alphatreeIds_)[i],daybefore,sampleSize,0,signNum, cacheSize);
        }

        int targetAlphatreeId = af_->useAlphaTree();
        af_->decode(targetAlphatreeId, "_target", target);

        //填写目标
        AlphaSignIterator targetIter(af_, "_target", signName, targetAlphatreeId, daybefore, sampleSize, 0, signNum, cacheSize);

        int fid = 0;
        for(int i = 0; i < signNum; ++i){
            pTarget[i] = targetIter.getValue();
            ++targetIter;
            for(int j = 0; j < features.getSize(); ++j){
                pFeature[fid++] = features[j]->getValue();
                features[j]->skip(1);
            }
        }

        //删除特征
        for(size_t i = 0; i < features.getSize(); ++i)
            delete features[i];
        return signNum;
    }

    void getFirstFeatureGains(size_t daybefore, size_t sampleSize, const char* weight, const char* target, const char* signName, float* gains,
                              int barSize = 64, int cacheSize = 4096){
        size_t signNum = af_->getAlphaDataBase()->getSignNum(daybefore, sampleSize, signName);
        //填写特征
        Vector<IBaseIterator<float>*> features( featureList_->getSize());
        for(size_t i = 0; i < features.getSize(); ++i){
            //cout<<(*alphaNameList_)[i].c_str()<<" "<<daybefore<<" "<<sampleSize<<endl;
            features[i] = new AlphaSignIterator(af_, (*alphaNameList_)[i].c_str(), signName, (*alphatreeIds_)[i],daybefore,sampleSize,0,signNum, cacheSize);
        }

        int targetAlphatreeId = af_->useAlphaTree();
        af_->decode(targetAlphatreeId, "_target", target);

        int weightAlphatreeId = -1;
        float* weightMemory = nullptr;
        IBaseIterator<float>* weightIter;
        if(weight != nullptr){
            weightAlphatreeId = af_->useAlphaTree();
            af_->decode(weightAlphatreeId, "_weight", weight);
            weightIter = new AlphaSignIterator(af_, "_weight", signName, weightAlphatreeId, daybefore, sampleSize, 0, signNum, cacheSize);
        } else {
            weightMemory = new float[signNum];
            weightIter = new MemoryIterator<float>(weightMemory, signNum);
            ((MemoryIterator<float>*)weightIter)->initialize(1);
        }


        //填写目标
        AlphaSignIterator targetIter(af_, "_target", signName, targetAlphatreeId, daybefore, sampleSize, 0, signNum, cacheSize);

        //申请中间内存
        float* memory = new float[3 * signNum];
        //分配2份内存，第一份是真实用的，第二份是中间结果，第三份是初始取样
        int* skip = new int[3 * signNum];
        MemoryIterator<float> pred(memory, signNum);
        MemoryIterator<float> g(memory + 1 * signNum, signNum);
        MemoryIterator<float> h(memory + 2 * signNum, signNum);
        MemoryIterator<bool> flag((bool*)(skip + 2 * signNum), signNum);
        flag.initialize(true);

        gbdt->getFirstFeatureGains(&features, weightIter, &targetIter, &g, &h, skip, &flag, &pred, gains, lossFunName, barSize);

        //释放空间
        delete []memory;
        delete []skip;

        //删除特征
        for(size_t i = 0; i < features.getSize(); ++i)
            delete features[i];
        af_->releaseAlphaTree(targetAlphatreeId);
        if(weightAlphatreeId != -1)
            af_->releaseAlphaTree(weightAlphatreeId);
        delete weightIter;
        if(weightMemory)
            delete []weightMemory;
    }

    void train(size_t daybefore, size_t sampleSize, const char* weight, const char* target, const char* signName,
               int barSize = 64, float minWeight = 32, int maxDepth = 16, float samplePercent = 1, float featurePercent = 1, int boostNum = 2, float boostWeightScale = 1, int cacheSize = 4096){
        size_t signNum = af_->getAlphaDataBase()->getSignNum(daybefore, sampleSize, signName);
        //填写特征
        Vector<IBaseIterator<float>*> features( featureList_->getSize());
        for(size_t i = 0; i < features.getSize(); ++i){
            //cout<<(*alphaNameList_)[i].c_str()<<" "<<daybefore<<" "<<sampleSize<<endl;
            features[i] = new AlphaSignIterator(af_, (*alphaNameList_)[i].c_str(), signName, (*alphatreeIds_)[i],daybefore,sampleSize,0,signNum, cacheSize);
        }

        //cout<<"target:"<<target<<endl;
        int targetAlphatreeId = af_->useAlphaTree();
        af_->decode(targetAlphatreeId, "_target", target);

        int weightAlphatreeId = -1;
        float* weightMemory = nullptr;
        IBaseIterator<float>* weightIter;
        if(weight != nullptr){
            weightAlphatreeId = af_->useAlphaTree();
            af_->decode(weightAlphatreeId, "_weight", weight);
            weightIter = new AlphaSignIterator(af_, "_weight", signName, weightAlphatreeId, daybefore, sampleSize, 0, signNum, cacheSize);
        } else {
            weightMemory = new float[signNum];
            weightIter = new MemoryIterator<float>(weightMemory, signNum);
            ((MemoryIterator<float>*)weightIter)->initialize(1);
        }


        //填写目标
        AlphaSignIterator targetIter(af_, "_target", signName, targetAlphatreeId, daybefore, sampleSize, 0, signNum, cacheSize);

        //申请中间内存
        float* memory = new float[3 * signNum];
        //分配2份内存，第一份是真实用的，第二份是中间结果
        int* skip = new int[3 * signNum];
        MemoryIterator<float> pred(memory, signNum);
        MemoryIterator<float> g(memory + 1 * signNum, signNum);
        MemoryIterator<float> h(memory + 2 * signNum, signNum);
        MemoryIterator<bool> flag((bool*)(skip + 2 * signNum), signNum);

        gbdt->train(&features, weightIter, &targetIter, &pred, &g, &h, skip, &flag, lossFunName, barSize, minWeight, maxDepth, samplePercent, featurePercent, boostNum, boostWeightScale);

        //释放空间
        delete []memory;
        delete []skip;

        //删除特征
        for(size_t i = 0; i < features.getSize(); ++i)
            delete features[i];
        af_->releaseAlphaTree(targetAlphatreeId);
        if(weightAlphatreeId != -1)
            af_->releaseAlphaTree(weightAlphatreeId);
        delete weightIter;
        if(weightMemory)
            delete []weightMemory;
    }

    float trainAndEval(size_t daybefore, size_t sampleSize, size_t evalDaybefore, size_t evalSampleSize, const char* weight, const char* target, const char* signName,
              int barSize = 64, float minWeight = 32, int maxDepth = 16, float samplePercent = 1, float featurePercent = 1, int boostNum = 2, float boostWeightScale = 1, int cacheSize = 4096){
        size_t signNum = af_->getAlphaDataBase()->getSignNum(daybefore, sampleSize, signName);
        size_t evalSignNum = af_->getAlphaDataBase()->getSignNum(evalDaybefore, evalSampleSize, signName);
        //填写特征
        Vector<IBaseIterator<float>*> features( featureList_->getSize());
        Vector<IBaseIterator<float>*> evalFeatures(featureList_->getSize());
        for(size_t i = 0; i < features.getSize(); ++i){
            //cout<<(*alphaNameList_)[i].c_str()<<" "<<daybefore<<" "<<sampleSize<<endl;
            features[i] = new AlphaSignIterator(af_, (*alphaNameList_)[i].c_str(), signName, (*alphatreeIds_)[i],daybefore,sampleSize,0,signNum, cacheSize);
            evalFeatures[i] = new AlphaSignIterator(af_, (*alphaNameList_)[i].c_str(), signName, (*alphatreeIds_)[i], evalDaybefore, evalSampleSize, 0, evalSignNum, cacheSize);
        }

        //cout<<"target:"<<target<<endl;
        int targetAlphatreeId = af_->useAlphaTree();
        af_->decode(targetAlphatreeId, "_target", target);

        int weightAlphatreeId = -1;
        float* weightMemory = nullptr;
        IBaseIterator<float>* weightIter;
        if(weight != nullptr){
            weightAlphatreeId = af_->useAlphaTree();
            af_->decode(weightAlphatreeId, "_weight", weight);
            weightIter = new AlphaSignIterator(af_, "_weight", signName, weightAlphatreeId, daybefore, sampleSize, 0, signNum, cacheSize);
        } else {
            weightMemory = new float[signNum];
            weightIter = new MemoryIterator<float>(weightMemory, signNum);
            ((MemoryIterator<float>*)weightIter)->initialize(1);
        }


        //填写目标
        AlphaSignIterator targetIter(af_, "_target", signName, targetAlphatreeId, daybefore, sampleSize, 0, signNum, cacheSize);
        AlphaSignIterator evalTargetIter(af_, "_target", signName, targetAlphatreeId, evalDaybefore, evalSampleSize, 0, evalSignNum, cacheSize);

        //申请中间内存
        float* memory = new float[3 * signNum + evalSignNum];
        //分配2份内存，第一份是真实用的，第二份是中间结果
        int* skip = new int[3 * max(signNum, evalSignNum)];
        MemoryIterator<float> pred(memory, signNum);
        MemoryIterator<float> evalPred(memory + 3 * signNum, evalSignNum);
        MemoryIterator<float> g(memory + 1 * signNum, signNum);
        MemoryIterator<float> h(memory + 2 * signNum, signNum);
        MemoryIterator<bool> flag((bool*)(skip + 2 * signNum), signNum);

        float evalLoss = gbdt->trainAndEval(&features, weightIter, &targetIter, &pred, &evalFeatures, &evalTargetIter, &evalPred, &g, &h, skip, &flag, lossFunName, barSize, minWeight, maxDepth, samplePercent, featurePercent, boostNum, boostWeightScale);
        //gbdt->train(&features, weightIter, &targetIter, &pred, &g, &h, skip, &flag, lossFunName, barSize, minWeight, maxDepth, samplePercent, featurePercent, boostNum, boostWeightScale);

        //释放空间
        delete []memory;
        delete []skip;

        //删除特征
        for(size_t i = 0; i < features.getSize(); ++i){
            delete features[i];
            delete evalFeatures[i];
        }

        af_->releaseAlphaTree(targetAlphatreeId);
        if(weightAlphatreeId != -1)
            af_->releaseAlphaTree(weightAlphatreeId);
        delete weightIter;
        if(weightMemory)
            delete []weightMemory;
        return evalLoss;
    }

    float eval(size_t evalDaybefore, size_t evalSampleSize, const char* target, const char* signName, int cacheSize = 4096){
        size_t evalSignNum = af_->getAlphaDataBase()->getSignNum(evalDaybefore, evalSampleSize, signName);
        //cout<<evalDaybefore<<" "<<evalSampleSize<<" "<<evalSignNum<<endl;
        //填写特征
        Vector<IBaseIterator<float>*> evalFeatures(featureList_->getSize());
        for(size_t i = 0; i < evalFeatures.getSize(); ++i){
            evalFeatures[i] = new AlphaSignIterator(af_, (*alphaNameList_)[i].c_str(), signName, (*alphatreeIds_)[i], evalDaybefore, evalSampleSize, 0, evalSignNum, cacheSize);
        }

        //cout<<"target:"<<target<<endl;
        int targetAlphatreeId = af_->useAlphaTree();
        af_->decode(targetAlphatreeId, "_target", target);

        //填写目标
        AlphaSignIterator evalTargetIter(af_, "_target", signName, targetAlphatreeId, evalDaybefore, evalSampleSize, 0, evalSignNum, cacheSize);

        //申请中间内存
        float* memory = new float[evalSignNum];
        //分配2份内存，第一份是真实用的，第二份是中间结果
        int* skip = new int[3 * evalSignNum];
        MemoryIterator<float> evalPred(memory, evalSignNum);


        float evalLoss = gbdt->eval( &evalFeatures, &evalTargetIter, &evalPred, skip, lossFunName);
        //gbdt->train(&features, weightIter, &targetIter, &pred, &g, &h, skip, &flag, lossFunName, barSize, minWeight, maxDepth, samplePercent, featurePercent, boostNum, boostWeightScale);

        /*{//test
            int* ids = new int[evalSignNum];
            AlphaForest::getAlphaforest()->getAlphaDataBase()->getStockIds(evalDaybefore, evalSampleSize, signName, ids);

            int cnt = 0;
            int id = 0;
            while (evalPred.isValid()){
                float v = evalPred.getValue();
                float t = evalTargetIter.getValue();
                v = 1.0f / (1.0f + expf(-v));
                if(v > 0.9){
                    ++cnt;
                    cout<<AlphaForest::getAlphaforest()->getAlphaDataBase()->getCode(ids[id])<<" "<<t<<endl;
                }

                evalPred.skip(1);
                evalTargetIter.skip(1);
                id++;
            }
            evalPred.skip(0, false);
            evalTargetIter.skip(0, false);
            cout<<"cnt = "<<cnt<<endl;
        }*/

        //释放空间
        delete []memory;
        delete []skip;

        //删除特征
        for(size_t i = 0; i < evalFeatures.getSize(); ++i){
            delete evalFeatures[i];
        }

        af_->releaseAlphaTree(targetAlphatreeId);
        return evalLoss;
    }

    void pred(size_t daybefore, size_t sampleSize, const char* signName, float* predOut, int cacheSize = 4096){
        size_t signNum = af_->getAlphaDataBase()->getSignNum(daybefore, sampleSize, signName);
        //填写特征
        Vector<IBaseIterator<float>*> features(featureList_->getSize());
        for(size_t i = 0; i < features.getSize(); ++i){
            //cout<<(*alphaNameList_)[i].c_str()<<" "<<daybefore<<" "<<sampleSize<<endl;
            features[i] = new AlphaSignIterator(af_, (*alphaNameList_)[i].c_str(), signName, (*alphatreeIds_)[i],daybefore,sampleSize,0,signNum, cacheSize);
        }

        //申请中间内存
        int* fmemory = new int[signNum * 2];

        MemoryIterator<float> pred(predOut, signNum);

        gbdt->pred(&features, fmemory, &pred, lossFunName);

        //删除特征
        for(size_t i = 0; i < features.getSize(); ++i)
            delete features[i];
        delete []fmemory;
    }

    int tostring(char* out){
        strcpy(out, gbdt->tostring(alphaNameList_).c_str());
        return strlen(out);
    }

    void saveModel(const char* path){
        gbdt->saveModel(path);
    }

    void loadModel(const char* path){
        gbdt->loadModel(path);
    }
protected:
    AlphaGBDT(AlphaForest* af, const char* alphatreeList, int alphatreeNum, float gamma, float lambda, int threadNum, const char* lossFunName = "binary:logistic"):af_(af){
            strcpy(this->lossFunName, lossFunName);
            gbdt = GBDT<>::create(threadNum, gamma, lambda);
            featureList_ = new Vector<AlphaSignIterator*>(alphatreeNum);
            alphatreeIds_ = new Vector<int>(alphatreeNum);
            alphaNameList_ = new Vector<string>(alphatreeNum);
            int equalIndex;
            for(int i = 0; i < alphatreeNum; ++i){
                int len = strlen(alphatreeList);
                equalIndex = 0;
                while (alphatreeList[equalIndex] != '=')
                    ++equalIndex;
                (*alphaNameList_)[i] = string(alphatreeList, 0, equalIndex);
                //cout<<(*alphaNameList_)[i].c_str()<<" "<<alphatreeList + (equalIndex + 1)<<endl;
                (*alphatreeIds_)[i] = af->useAlphaTree();
                af->decode((*alphatreeIds_)[i], (*alphaNameList_)[i].c_str(), alphatreeList + (equalIndex + 1));
                alphatreeList = alphatreeList + (len + 1);
            }

    }

    ~AlphaGBDT(){
        GBDT<>::release(gbdt);
        for(size_t i = 0; i < alphatreeIds_->getSize(); ++i)
            af_->releaseAlphaTree((*alphatreeIds_)[i]);
        delete featureList_;
        delete alphatreeIds_;
        delete alphaNameList_;
    }
    AlphaForest* af_;
    Vector<int>* alphatreeIds_;
    Vector<string>* alphaNameList_;
    Vector<AlphaSignIterator*>* featureList_;
    GBDT<>* gbdt;
    char lossFunName[128];

    static AlphaGBDT* alphaGBDT_;
};

AlphaGBDT *AlphaGBDT::alphaGBDT_ = nullptr;

#endif //ALPHATREE_ALPHAGBDT_H
