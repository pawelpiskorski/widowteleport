#include <map>
#include <string.h>
#include <stdio.h>

#define CONFIG_TAG_LENGTH 10
const wchar_t[CONFIG_TAG_LENGTH] CONFIG_TAG = L"INBUILTCFG"
#define CONFIG_SIZE_CHARS 3
#define KEY_LENGTH 10
#define VALUE_LENGTH 10

typedef wchar_t ConfigKey[KEY_LENGTH];
typedef wchar_t ConfigValue[VALUE_LENGTH];

struct KeyCompare
{
    bool operator()(const ConfigKey k1, const ConfigKey k2) const
    {
	    for (int i=0; i< KEY_LENGTH; i++)
	    {
	  	    if (k1[i]>k2[i])
	  	  	    return true;
	    }
	    return false;
    }
};

typedef std::map<ConfigKey, ConfigValue, KeyCompare> ConfigMap;

class BinaryPatcher
{
public:
	ConfigMap *Config;

	BinaryPatcher()
	{
		Config = new ConfigMap();
	}

	~BinaryPatcher()
	{
		delete Config;
	}


	void ParseConfig(const wchar_t* config)
	{
		unsigned pos= CONFIG_TAG_LENGTH;
		wchar_t *cursor=&config[pos];
		wchar_t temp;
		wchar_t *end;
		long configSize;

		//temp = cursor[CONFIG_SIZE_CHARS+1];
		//cursor[CONFIG_SIZE_CHARS+1] = 0;
		configSize = wcstol(cursor,&end,10);
		pos += CONFIG_SIZE_CHARS;
		cursor = &cursor[CONFIG_SIZE_CHARS];
		


	}

	bool ValueOf(ConfigKey key, ConfigValue value)
	{
		return false;
	}

	void SaveConfig(wchar_t* file)
	{


	}

};