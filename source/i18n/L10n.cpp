/* Copyright (c) 2013 Wildfire Games
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "precompiled.h"
#include "i18n/L10n.h"

#include <iostream>
#include <string>
#include <boost/concept_check.hpp>

#include "lib/file/file_system.h"
#include "lib/utf8.h"

#include "ps/CLogger.h"
#include "ps/ConfigDB.h"
#include "ps/Filesystem.h"
#include "ps/GameSetup/GameSetup.h"


L10n& L10n::instance()
{
	static L10n m_instance;
	return m_instance;
}

L10n::L10n()
: currentLocaleIsOriginalGameLocale(false) // determineCurrentLocale() takes care of setting this value to true.
{
	loadListOfAvailableLocales();
	setCurrentLocale(getConfiguredOrSystemLocale());
}

L10n::~L10n()
{
	delete dictionary;
}

Locale L10n::getCurrentLocale()
{
	return currentLocale;
}

void L10n::setCurrentLocale(const std::string& localeCode)
{
	setCurrentLocale(Locale(Locale::createCanonical(localeCode.c_str())));
}

void L10n::setCurrentLocale(Locale locale)
{
	bool reload = (currentLocale != locale) == TRUE;
	currentLocale = locale;
	currentLocaleIsOriginalGameLocale = (currentLocale == Locale::getUS()) == TRUE;
	if (reload && !currentLocaleIsOriginalGameLocale)
		loadDictionaryForCurrentLocale();
}

std::vector< std::string > L10n::getSupportedLocaleCodes()
{
	std::vector<std::string> supportedLocaleCodes;
	for (std::vector<Locale*>::iterator iterator = availableLocales.begin(); iterator != availableLocales.end(); ++iterator)
	{
		if (!InDevelopmentCopy() && strcmp((*iterator)->getBaseName(), "long") == 0)
			continue;
		supportedLocaleCodes.push_back((*iterator)->getBaseName());
	}
	return supportedLocaleCodes;
}

std::vector< std::wstring > L10n::getSupportedLocaleDisplayNames()
{
	std::vector<std::wstring> supportedLocaleDisplayNames;
	for (std::vector<Locale*>::iterator iterator = availableLocales.begin(); iterator != availableLocales.end(); ++iterator)
	{
		if (strcmp((*iterator)->getBaseName(), "long") == 0)
		{
			if (InDevelopmentCopy())
				supportedLocaleDisplayNames.push_back(wstring_from_utf8(L10n::instance().translate("Long strings")));
			continue;
		}

		UnicodeString utf16LocaleDisplayName;
		(**iterator).getDisplayName(**iterator, utf16LocaleDisplayName);
		std::string localeDisplayName;
		// TODO: this is probably unsafe in MSVC
		utf16LocaleDisplayName.toUTF8String(localeDisplayName);
		supportedLocaleDisplayNames.push_back(wstring_from_utf8(localeDisplayName));
	}
	return supportedLocaleDisplayNames;
}

int L10n::getCurrentLocaleIndex()
{
	std::vector<std::string> supportedLocaleCodes;

	// Try to match a whole code first.
	for (std::vector<Locale*>::iterator iterator = availableLocales.begin(); iterator != availableLocales.end(); ++iterator)
	{
		if (strcmp((**iterator).getBaseName(), getCurrentLocale().getBaseName()) == 0)
		{
			return iterator - availableLocales.begin();
		}
	}

	// Match language code only if no whole code matched.
	for (std::vector<Locale*>::iterator iterator = availableLocales.begin(); iterator != availableLocales.end(); ++iterator)
	{
		if (strcmp((**iterator).getLanguage(), getCurrentLocale().getLanguage()) == 0)
		{
			return iterator - availableLocales.begin();
		}
	}

	// Use en_US, the default locale.
	for (std::vector<Locale*>::iterator iterator = availableLocales.begin(); iterator != availableLocales.end(); ++iterator)
	{
		if (strcmp((**iterator).getLanguage(), Locale::getUS().getLanguage()) == 0)
		{
			return iterator - availableLocales.begin();
		}
	}

	return 0; // It should never get this far.
}

std::string L10n::translate(const std::string& sourceString)
{
	if (!currentLocaleIsOriginalGameLocale)
	{
		return dictionary->translate(sourceString);
	}
	else
	{
		return sourceString;
	}
}

std::string L10n::translateWithContext(const std::string& context, const std::string& sourceString)
{
	if (!currentLocaleIsOriginalGameLocale)
	{
		return dictionary->translate_ctxt(context, sourceString);
	}
	else
	{
		return sourceString;
	}
}

std::string L10n::translatePlural(const std::string& singularSourceString, const std::string& pluralSourceString, int number)
{
	if (!currentLocaleIsOriginalGameLocale)
	{
		return dictionary->translate_plural(singularSourceString, pluralSourceString, number);
	}
	else
	{
		if (number == 1)
		{
			return singularSourceString;
		}
		else
		{
			return pluralSourceString;
		}
	}
}

std::string L10n::translatePluralWithContext(const std::string& context, const std::string& singularSourceString, const std::string& pluralSourceString, int number)
{
	if (!currentLocaleIsOriginalGameLocale)
	{
		return dictionary->translate_ctxt_plural(context, singularSourceString, pluralSourceString, number);
	}
	else
	{
		if (number == 1)
		{
			return singularSourceString;
		}
		else
		{
			return pluralSourceString;
		}
	}
}

std::string L10n::translateLines(const std::string& sourceString)
{
	std::string targetString;
	std::stringstream stringOfLines(sourceString);
	std::string line;

	while (std::getline(stringOfLines, line)) {
		targetString.append(translate(line));
		targetString.append("\n");
	}

	return targetString;
}

UDate L10n::parseDateTime(const std::string& dateTimeString, const std::string& dateTimeFormat, const Locale& locale)
{
	UErrorCode success = U_ZERO_ERROR;
	UnicodeString utf16DateTimeString = UnicodeString::fromUTF8(dateTimeString.c_str());
	UnicodeString utf16DateTimeFormat = UnicodeString::fromUTF8(dateTimeFormat.c_str());

	DateFormat* dateFormatter = new SimpleDateFormat(utf16DateTimeFormat, locale, success);
	UDate date = dateFormatter->parse(utf16DateTimeString, success);
	delete dateFormatter;

	return date;
}

std::string L10n::localizeDateTime(const UDate& dateTime, DateTimeType type, DateFormat::EStyle style)
{
	UnicodeString utf16Date;
	std::string utf8Date;

	DateFormat* dateFormatter = createDateTimeInstance(type, style, currentLocale);
	dateFormatter->format(dateTime, utf16Date);
	// TODO: this is probably unsafe in MSVC
	utf16Date.toUTF8String(utf8Date);
	delete dateFormatter;

	return utf8Date;
}

std::string L10n::formatMillisecondsIntoDateString(int milliseconds, const std::string& formatString)
{
	UErrorCode success = U_ZERO_ERROR;
	std::string utf8Date;
	UnicodeString utf16Date;
	UnicodeString utf16SourceDateTimeFormat = UnicodeString::fromUTF8("A");
	UnicodeString utf16LocalizedDateTimeFormat = UnicodeString::fromUTF8(formatString.c_str());
	char buffer[32];
	sprintf_s(buffer, ARRAY_SIZE(buffer), "%d", milliseconds);
	UnicodeString utf16MillisecondsString = UnicodeString::fromUTF8(buffer);

	SimpleDateFormat* dateFormatter = new SimpleDateFormat(utf16SourceDateTimeFormat, currentLocale, success);
	UDate dateTime = dateFormatter->parse(utf16MillisecondsString, success);
	dateFormatter->applyLocalizedPattern(utf16LocalizedDateTimeFormat, success);
	dateFormatter->format(dateTime, utf16Date);
	// TODO: this is probably unsafe in MSVC
	utf16Date.toUTF8String(utf8Date);
	delete dateFormatter;

	return utf8Date;
}

std::string L10n::formatDecimalNumberIntoString(double number)
{
	UErrorCode success = U_ZERO_ERROR;
	UnicodeString utf16Number;
	std::string utf8Number;
	NumberFormat* numberFormatter = NumberFormat::createInstance(currentLocale, UNUM_DECIMAL, success);
	numberFormatter->format(number, utf16Number);
	// TODO: this is probably unsafe in MSVC
	utf16Number.toUTF8String(utf8Number);
	return utf8Number;
}

VfsPath L10n::localizePath(VfsPath sourcePath)
{
	VfsPath path = sourcePath;

	VfsPath localizedPath = sourcePath.Parent() / L"l10n" / wstring_from_utf8(currentLocale.getLanguage()) / sourcePath.Filename();
	if (VfsFileExists(localizedPath))
	{
		path = localizedPath;
	}

	return path;
}

Locale L10n::getConfiguredOrSystemLocale()
{
	std::string locale;
	CFG_GET_VAL("locale", String, locale);
	if (!locale.empty())
		return Locale(Locale::createCanonical(locale.c_str()));
	else
		return Locale::getDefault();
}

void L10n::loadDictionaryForCurrentLocale()
{
	delete dictionary;
	dictionary = new tinygettext::Dictionary();

	VfsPaths filenames;
	if (vfs::GetPathnames(g_VFS, L"l10n/", (wstring_from_utf8(currentLocale.getLanguage()) + L".*.po").c_str(), filenames) < 0)
		return;

	for (VfsPaths::iterator it = filenames.begin(); it != filenames.end(); ++it)
	{
		VfsPath filename = *it;
		CVFSFile file;
		file.Load(g_VFS, filename);
		std::string content = file.DecodeUTF8();
		readPoIntoDictionary(content, dictionary);
	}
}

void L10n::loadListOfAvailableLocales()
{
	for (std::vector<Locale*>::iterator iterator = availableLocales.begin(); iterator != availableLocales.end(); ++iterator)
	{
		delete *iterator;
	}
	availableLocales.clear();

	Locale* defaultLocale = new Locale(Locale::getUS());
	availableLocales.push_back(defaultLocale); // Always available.

	VfsPaths filenames;
	if (vfs::GetPathnames(g_VFS, L"l10n/", L"*.po", filenames) < 0)
		return;

	for (VfsPaths::iterator it = filenames.begin(); it != filenames.end(); ++it)
	{
		// Note: PO files follow this naming convention: “l10n/<locale code>.<mod name>.po”. For example: “l10n/gl.public.po”.
		VfsPath filepath = *it;
		std::string filename = utf8_from_wstring(filepath.string()).substr(strlen("l10n/"));
		std::size_t lengthToFirstDot = filename.find('.');
		std::string localeCode = filename.substr(0, lengthToFirstDot);
		Locale* locale = new Locale(Locale::createCanonical(localeCode.c_str()));

		bool localeIsAlreadyAvailable = false;
		for (std::vector<Locale*>::iterator iterator = availableLocales.begin(); iterator != availableLocales.end(); ++iterator)
		{
			if (*locale == **iterator)
			{
				localeIsAlreadyAvailable = true;
				break;
			}
		}

		if (!localeIsAlreadyAvailable)
		{
			availableLocales.push_back(locale);
		}
	}
}

void L10n::readPoIntoDictionary(const std::string& poContent, tinygettext::Dictionary* dictionary)
{
	try
	{
		std::istringstream inputStream(poContent);
		tinygettext::POParser::parse("virtual PO file", inputStream, *dictionary, false);
	}
	catch(std::exception& e)
	{
		LOGERROR(L"[Localization] Exception while reading virtual PO file: %hs", e.what());
	}
}

DateFormat* L10n::createDateTimeInstance(L10n::DateTimeType type, DateFormat::EStyle style, const Locale& locale)
{
	if (type == Date)
	{
		return SimpleDateFormat::createDateInstance(style, locale);
	}
	else if (type == Time)
	{
		return SimpleDateFormat::createTimeInstance(style, locale);
	}
	else // Provide DateTime by default.
	{
		return SimpleDateFormat::createDateTimeInstance(style, style, locale);
	}
}
