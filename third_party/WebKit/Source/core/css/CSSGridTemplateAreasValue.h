/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSGridTemplateAreasValue_h
#define CSSGridTemplateAreasValue_h

#include "core/css/CSSValue.h"
#include "core/style/GridArea.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

class CSSGridTemplateAreasValue : public CSSValue {
 public:
  static CSSGridTemplateAreasValue* Create(
      const NamedGridAreaMap& grid_area_map,
      size_t row_count,
      size_t column_count) {
    return new CSSGridTemplateAreasValue(grid_area_map, row_count,
                                         column_count);
  }
  ~CSSGridTemplateAreasValue() {}

  String CustomCSSText() const;

  const NamedGridAreaMap& GridAreaMap() const { return grid_area_map_; }
  size_t RowCount() const { return row_count_; }
  size_t ColumnCount() const { return column_count_; }

  bool Equals(const CSSGridTemplateAreasValue&) const;

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  CSSGridTemplateAreasValue(const NamedGridAreaMap&,
                            size_t row_count,
                            size_t column_count);

  NamedGridAreaMap grid_area_map_;
  size_t row_count_;
  size_t column_count_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSGridTemplateAreasValue,
                            IsGridTemplateAreasValue());

}  // namespace blink

#endif  // CSSGridTemplateAreasValue_h
