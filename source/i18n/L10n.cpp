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
	: currentLocaleIsOriginalGameLocale(false), dictionary(new tinygettext::Dictionary())
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

std::vector<std::string> L10n::getSupportedLocaleCodes()
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

std::vector<std::wstring> L10n::getSupportedLocaleDisplayNames()
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
		char localeDisplayName[512];
		CheckedArrayByteSink sink(localeDisplayName, ARRAY_SIZE(localeDisplayName));
		utf16LocaleDisplayName.toUTF8(sink);
		ENSURE(!sink.Overflowed());

		supportedLocaleDisplayNames.push_back(wstring_from_utf8(std::string(localeDisplayName, sink.NumberOfBytesWritten())));
	}
	return supportedLocaleDisplayNames;
}

int L10n::getCurrentLocaleIndex()
{
	int languageCodeOnly = -1;
	int defaultLocale = -1;

	// Check for an exact match (whole code) first, then for the same language and at last for the default locale en_US
	for (std::vector<Locale*>::iterator iterator = availableLocales.begin(); iterator != availableLocales.end(); ++iterator)
	{
		if (strcmp((**iterator).getBaseName(), getCurrentLocale().getBaseName()) == 0)
			return iterator - availableLocales.begin();

		if (languageCodeOnly < 0 && strcmp((**iterator).getLanguage(), getCurrentLocale().getLanguage()) == 0)
			languageCodeOnly = iterator - availableLocales.begin();

		if (defaultLocale < 0 && strcmp((**iterator).getLanguage(), Locale::getUS().getLanguage()) == 0)
			defaultLocale = iterator - availableLocales.begin();
	}

	if (languageCodeOnly >= 0)
		return languageCodeOnly;
	
	if (defaultLocale >= 0)
		return defaultLocale;

	debug_warn(L"Current locale, one with the same language, and the default language (en_US) not found in the available locales.");
	return 0;
}

std::string L10n::translate(const std::string& sourceString)
{
	if (!currentLocaleIsOriginalGameLocale)
		return dictionary->translate(sourceString.c_str());

	return sourceString;
}

std::string L10n::translateWithContext(const std::string& context, const std::string& sourceString)
{
	if (!currentLocaleIsOriginalGameLocale)
		return dictionary->translate_ctxt(context.c_str(), sourceString.c_str());

	return sourceString;
}

std::string L10n::translatePlural(const std::string& singularSourceString, const std::string& pluralSourceString, int number)
{
	if (!currentLocaleIsOriginalGameLocale)
		return dictionary->translate_plural(singularSourceString.c_str(), pluralSourceString.c_str(), number);

	if (number == 1)
		return singularSourceString;

	return pluralSourceString;
}

std::string L10n::translatePluralWithContext(const std::string& context, const std::string& singularSourceString, const std::string& pluralSourceString, int number)
{
	if (!currentLocaleIsOriginalGameLocale)
		return dictionary->translate_ctxt_plural(context.c_str(), singularSourceString.c_str(), pluralSourceString.c_str(), number);

	if (number == 1)
		return singularSourceString;

	return pluralSourceString;
}

std::string L10n::translateLines(const std::string& sourceString)
{
	std::string targetString;
	std::stringstream stringOfLines(sourceString);
	std::string line;

	while (std::getline(stringOfLines, line)) {
		targetString.append(translate(line.c_str()));
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

	DateFormat* dateFormatter = createDateTimeInstance(type, style, currentLocale);
	dateFormatter->format(dateTime, utf16Date);
	char utf8Date[512];
	CheckedArrayByteSink sink(utf8Date, ARRAY_SIZE(utf8Date));
	utf16Date.toUTF8(sink);
	ENSURE(!sink.Overflowed());
	delete dateFormatter;

	return std::string(utf8Date, sink.NumberOfBytesWritten());
}

std::string L10n::formatMillisecondsIntoDateString(int milliseconds, const std::string& formatString)
{
	UErrorCode success = U_ZERO_ERROR;
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
	char utf8Date[512];
	CheckedArrayByteSink sink(utf8Date, ARRAY_SIZE(utf8Date));
	utf16Date.toUTF8(sink);
	ENSURE(!sink.Overflowed());
	delete dateFormatter;

	return std::string(utf8Date, sink.NumberOfBytesWritten());
}

std::string L10n::formatDecimalNumberIntoString(double number)
{
	UErrorCode success = U_ZERO_ERROR;
	UnicodeString utf16Number;
	NumberFormat* numberFormatter = NumberFormat::createInstance(currentLocale, UNUM_DECIMAL, success);
	numberFormatter->format(number, utf16Number);
	char utf8Number[512];
	CheckedArrayByteSink sink(utf8Number, ARRAY_SIZE(utf8Number));
	utf16Number.toUTF8(sink);
	ENSURE(!sink.Overflowed());

	return std::string(utf8Number, sink.NumberOfBytesWritten());
}

VfsPath L10n::localizePath(VfsPath sourcePath)
{
	VfsPath path = sourcePath;

	VfsPath localizedPath = sourcePath.Parent() / L"l10n" / wstring_from_utf8(currentLocale.getLanguage()) / sourcePath.Filename();
	if (VfsFileExists(localizedPath))
		path = localizedPath;

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
		delete *iterator;

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
			availableLocales.push_back(locale);
	}
}

void L10n::readPoIntoDictionary(const std::string& poContent, tinygettext::Dictionary* dictionary)
{
	// TODO TODO TODO exceptions are not safe... but parse does not seem to throw anything (there is a catch in that while)
	try
	{
		tinygettext::POParser::parse("virtual PO file", poContent.c_str(), *dictionary, false);
	}
	catch(std::exception& e)
	{
		LOGERROR(L"[Localization] Exception while reading virtual PO file: %hs", e.what());
	}
}

DateFormat* L10n::createDateTimeInstance(L10n::DateTimeType type, DateFormat::EStyle style, const Locale& locale)
{
	switch(type)
	{
	case Date:
		return SimpleDateFormat::createDateInstance(style, locale);

	case Time:
		return SimpleDateFormat::createTimeInstance(style, locale);

	case DateTime:
	default:
		return SimpleDateFormat::createDateTimeInstance(style, style, locale);
	}
}
